/*
 * DIB driver GDI objects.
 *
 * Copyright 2011 Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

/*
 *
 * Decompose the 16 ROP2s into an expression of the form
 *
 * D = (D & A) ^ X
 *
 * Where A and X depend only on P (and so can be precomputed).
 *
 *                                       A    X
 *
 * R2_BLACK         0                    0    0
 * R2_NOTMERGEPEN   ~(D | P)            ~P   ~P
 * R2_MASKNOTPEN    ~P & D              ~P    0
 * R2_NOTCOPYPEN    ~P                   0   ~P
 * R2_MASKPENNOT    P & ~D               P    P
 * R2_NOT           ~D                   1    1
 * R2_XORPEN        P ^ D                1    P
 * R2_NOTMASKPEN    ~(P & D)             P    1
 * R2_MASKPEN       P & D                P    0
 * R2_NOTXORPEN     ~(P ^ D)             1   ~P
 * R2_NOP           D                    1    0
 * R2_MERGENOTPEN   ~P | D               P   ~P
 * R2_COPYPEN       P                    0    P
 * R2_MERGEPENNOT   P | ~D              ~P    1
 * R2_MERGEPEN      P | D               ~P    P
 * R2_WHITE         1                    0    1
 *
 */

/* A = (P & A1) | (~P & A2) */
#define ZERO {0, 0}
#define ONE {0xffffffff, 0xffffffff}
#define P {0xffffffff, 0}
#define NOT_P {0, 0xffffffff}

static const DWORD rop2_and_array[16][2] =
{
    ZERO, NOT_P, NOT_P, ZERO,
    P,    ONE,   ONE,   P,
    P,    ONE,   ONE,   P,
    ZERO, NOT_P, NOT_P, ZERO
};

/* X = (P & X1) | (~P & X2) */
static const DWORD rop2_xor_array[16][2] =
{
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE,
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE
};

#undef NOT_P
#undef P
#undef ONE
#undef ZERO

void calc_and_xor_masks(INT rop, DWORD color, DWORD *and, DWORD *xor)
{
    /* NB The ROP2 codes start at one and the arrays are zero-based */
    *and = (color & rop2_and_array[rop-1][0]) | ((~color) & rop2_and_array[rop-1][1]);
    *xor = (color & rop2_xor_array[rop-1][0]) | ((~color) & rop2_xor_array[rop-1][1]);
}

static inline void order_end_points(int *s, int *e)
{
    if(*s > *e)
    {
        int tmp;
        tmp = *s + 1;
        *s = *e + 1;
        *e = tmp;
    }
}

static inline BOOL pt_in_rect( const RECT *rect, POINT pt )
{
    return ((pt.x >= rect->left) && (pt.x < rect->right) &&
            (pt.y >= rect->top) && (pt.y < rect->bottom));
}

static BOOL solid_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end)
{
    RECT rc;
    DC *dc = get_dibdrv_dc( &pdev->dev );

    if(get_clip_region(dc) || !pt_in_rect(&dc->vis_rect, *start) || !pt_in_rect(&dc->vis_rect, *end))
        return FALSE;

    rc.left   = start->x;
    rc.top    = start->y;
    rc.right  = end->x;
    rc.bottom = end->y;

    if(rc.top == rc.bottom)
    {
        order_end_points(&rc.left, &rc.right);
        rc.bottom++;
        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rc, pdev->pen_and, pdev->pen_xor);
        return TRUE;
    }
    else if(rc.left == rc.right)
    {
        order_end_points(&rc.top, &rc.bottom);
        rc.right++;
        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rc, pdev->pen_and, pdev->pen_xor);
        return TRUE;
    }

    return FALSE;
}

/***********************************************************************
 *           dibdrv_SelectPen
 */
HPEN CDECL dibdrv_SelectPen( PHYSDEV dev, HPEN hpen )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSelectPen );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGPEN logpen;

    TRACE("(%p, %p)\n", dev, hpen);

    if (!GetObjectW( hpen, sizeof(logpen), &logpen ))
    {
        /* must be an extended pen */
        EXTLOGPEN *elp;
        INT size = GetObjectW( hpen, 0, NULL );

        if (!size) return 0;

        elp = HeapAlloc( GetProcessHeap(), 0, size );

        GetObjectW( hpen, size, elp );
        /* FIXME: add support for user style pens */
        logpen.lopnStyle = elp->elpPenStyle;
        logpen.lopnWidth.x = elp->elpWidth;
        logpen.lopnWidth.y = 0;
        logpen.lopnColor = elp->elpColor;

        HeapFree( GetProcessHeap(), 0, elp );
    }

    if (hpen == GetStockObject( DC_PEN ))
        logpen.lopnColor = GetDCPenColor( dev->hdc );

    pdev->pen_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, logpen.lopnColor);
    calc_and_xor_masks(GetROP2(dev->hdc), pdev->pen_color, &pdev->pen_and, &pdev->pen_xor);

    pdev->defer |= DEFER_PEN;

    switch(logpen.lopnStyle & PS_STYLE_MASK)
    {
    case PS_SOLID:
        if(logpen.lopnStyle & PS_GEOMETRIC) break;
        if(logpen.lopnWidth.x > 1) break;
        pdev->pen_line = solid_pen_line;
        pdev->defer &= ~DEFER_PEN;
        break;
    default:
        break;
    }

    return next->funcs->pSelectPen( next, hpen );
}

/***********************************************************************
 *           dibdrv_SetDCPenColor
 */
COLORREF CDECL dibdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDCPenColor );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_PEN) == GetStockObject( DC_PEN ))
    {
        pdev->pen_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, color);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->pen_color, &pdev->pen_and, &pdev->pen_xor);
    }

    return next->funcs->pSetDCPenColor( next, color );
}

/**********************************************************************
 *             solid_brush
 *
 * Fill a number of rectangles with the solid brush
 * FIXME: Should we insist l < r && t < b?  Currently we assume this.
 */
static BOOL solid_brush(dibdrv_physdev *pdev, int num, RECT *rects)
{
    int i;
    DC *dc = get_dibdrv_dc( &pdev->dev );

    if(get_clip_region(dc)) return FALSE;

    for(i = 0; i < num; i++) /* simple clip to extents */
    {
        if(rects[i].left   < dc->vis_rect.left)   rects[i].left   = dc->vis_rect.left;
        if(rects[i].top    < dc->vis_rect.top)    rects[i].top    = dc->vis_rect.top;
        if(rects[i].right  > dc->vis_rect.right)  rects[i].right  = dc->vis_rect.right;
        if(rects[i].bottom > dc->vis_rect.bottom) rects[i].bottom = dc->vis_rect.bottom;
    }

    pdev->dib.funcs->solid_rects(&pdev->dib, num, rects, pdev->brush_and, pdev->brush_xor);
    return TRUE;
}

void update_brush_rop( dibdrv_physdev *pdev, INT rop )
{
    if(pdev->brush_style == BS_SOLID)
        calc_and_xor_masks(rop, pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
}

/***********************************************************************
 *           dibdrv_SelectBrush
 */
HBRUSH CDECL dibdrv_SelectBrush( PHYSDEV dev, HBRUSH hbrush )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSelectBrush );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGBRUSH logbrush;

    TRACE("(%p, %p)\n", dev, hbrush);

    if (!GetObjectW( hbrush, sizeof(logbrush), &logbrush )) return 0;

    if (hbrush == GetStockObject( DC_BRUSH ))
        logbrush.lbColor = GetDCBrushColor( dev->hdc );

    pdev->brush_style = logbrush.lbStyle;

    pdev->defer |= DEFER_BRUSH;

    switch(logbrush.lbStyle)
    {
    case BS_SOLID:
        pdev->brush_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, logbrush.lbColor);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
        pdev->brush_rects = solid_brush;
        pdev->defer &= ~DEFER_BRUSH;
        break;
    default:
        break;
    }

    return next->funcs->pSelectBrush( next, hbrush );
}

/***********************************************************************
 *           dibdrv_SetDCBrushColor
 */
COLORREF CDECL dibdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDCBrushColor );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_BRUSH) == GetStockObject( DC_BRUSH ))
    {
        pdev->brush_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, color);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
    }

    return next->funcs->pSetDCBrushColor( next, color );
}