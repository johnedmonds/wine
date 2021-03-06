/*
 * DIB driver initialization and DC functions.
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

#include <assert.h>

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

static const DWORD bit_fields_888[3] = {0xff0000, 0x00ff00, 0x0000ff};
static const DWORD bit_fields_555[3] = {0x7c00, 0x03e0, 0x001f};

static void calc_shift_and_len(DWORD mask, int *shift, int *len)
{
    int s, l;

    if(!mask)
    {
        *shift = *len = 0;
        return;
    }

    s = 0;
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        s++;
    }
    l = 0;
    while ((mask & 1) == 1)
    {
        mask >>= 1;
        l++;
    }
    *shift = s;
    *len = l;
}

static void init_bit_fields(dib_info *dib, const DWORD *bit_fields)
{
    dib->red_mask    = bit_fields[0];
    dib->green_mask  = bit_fields[1];
    dib->blue_mask   = bit_fields[2];
    calc_shift_and_len(dib->red_mask,   &dib->red_shift,   &dib->red_len);
    calc_shift_and_len(dib->green_mask, &dib->green_shift, &dib->green_len);
    calc_shift_and_len(dib->blue_mask,  &dib->blue_shift,  &dib->blue_len);
}

static BOOL init_dib_info(dib_info *dib, const BITMAPINFOHEADER *bi, const DWORD *bit_fields,
                          RGBQUAD *color_table, int color_table_size, void *bits, enum dib_info_flags flags)
{
    dib->bit_count = bi->biBitCount;
    dib->width     = bi->biWidth;
    dib->height    = bi->biHeight;
    dib->stride    = get_dib_stride( dib->width, dib->bit_count );
    dib->bits      = bits;
    dib->ptr_to_free = NULL;
    dib->flags     = flags;

    if(dib->height < 0) /* top-down */
    {
        dib->height = -dib->height;
    }
    else /* bottom-up */
    {
        /* bits always points to the top-left corner and the stride is -ve */
        dib->bits    = (BYTE*)dib->bits + (dib->height - 1) * dib->stride;
        dib->stride  = -dib->stride;
    }

    dib->funcs = &funcs_null;

    switch(dib->bit_count)
    {
    case 32:
        if(bi->biCompression == BI_RGB)
            bit_fields = bit_fields_888;

        init_bit_fields(dib, bit_fields);

        if(dib->red_mask == 0xff0000 && dib->green_mask == 0x00ff00 && dib->blue_mask == 0x0000ff)
            dib->funcs = &funcs_8888;
        else
            dib->funcs = &funcs_32;
        break;

    case 24:
        dib->funcs = &funcs_24;
        break;

    case 16:
        if(bi->biCompression == BI_RGB)
            bit_fields = bit_fields_555;

        init_bit_fields(dib, bit_fields);

        if(dib->red_mask == 0x7c00 && dib->green_mask == 0x03e0 && dib->blue_mask == 0x001f)
            dib->funcs = &funcs_555;
        else
            dib->funcs = &funcs_16;
        break;

    case 8:
        dib->funcs = &funcs_8;
        break;

    case 4:
        dib->funcs = &funcs_4;
        break;

    case 1:
        dib->funcs = &funcs_1;
        break;

    default:
        TRACE("bpp %d not supported, will forward to graphics driver.\n", dib->bit_count);
        return FALSE;
    }

    if(color_table)
    {
        if (flags & private_color_table)
        {
            dib->color_table = HeapAlloc(GetProcessHeap(), 0, color_table_size * sizeof(dib->color_table[0]));
            if(!dib->color_table) return FALSE;
            memcpy(dib->color_table, color_table, color_table_size * sizeof(color_table[0]));
        }
        else
            dib->color_table = color_table;
        dib->color_table_size = color_table_size;
    }
    else
    {
        dib->color_table = NULL;
        dib->color_table_size = 0;
    }

    return TRUE;
}

BOOL init_dib_info_from_packed(dib_info *dib, const BITMAPINFOHEADER *bi, WORD usage, HPALETTE palette)
{
    DWORD *masks = NULL;
    RGBQUAD *color_table = NULL, pal_table[256];
    BYTE *ptr = (BYTE*)bi + bi->biSize;
    int num_colors = get_dib_num_of_colors( (const BITMAPINFO *)bi );

    if(bi->biCompression == BI_BITFIELDS)
    {
        masks = (DWORD *)ptr;
        ptr += 3 * sizeof(DWORD);
    }

    if(num_colors)
    {
        if(usage == DIB_PAL_COLORS)
        {
            PALETTEENTRY entries[256];
            const WORD *index = (const WORD*) ptr;
            UINT i, count = GetPaletteEntries( palette, 0, num_colors, entries );
            for (i = 0; i < num_colors; i++, index++)
            {
                PALETTEENTRY *entry = &entries[*index % count];
                pal_table[i].rgbRed      = entry->peRed;
                pal_table[i].rgbGreen    = entry->peGreen;
                pal_table[i].rgbBlue     = entry->peBlue;
                pal_table[i].rgbReserved = 0;
            }
            color_table = pal_table;
            ptr += num_colors * sizeof(WORD);
        }
        else
        {
            color_table = (RGBQUAD*)ptr;
            ptr += num_colors * sizeof(*color_table);
        }
    }

    return init_dib_info(dib, bi, masks, color_table, num_colors, ptr, private_color_table);
}

BOOL init_dib_info_from_bitmapinfo(dib_info *dib, const BITMAPINFO *info, void *bits, enum dib_info_flags flags)
{
    unsigned int colors = get_dib_num_of_colors( info );
    void *colorptr = (char *)&info->bmiHeader + info->bmiHeader.biSize;
    const DWORD *bitfields = (info->bmiHeader.biCompression == BI_BITFIELDS) ? (DWORD *)colorptr : NULL;

    return init_dib_info( dib, &info->bmiHeader, bitfields, colors ? colorptr : NULL, colors, bits, flags );
}

static void clear_dib_info(dib_info *dib)
{
    dib->color_table = NULL;
    dib->bits = NULL;
    dib->ptr_to_free = NULL;
}

/**********************************************************************
 *      free_dib_info
 *
 * Free the resources associated with a dib and optionally the bits
 */
void free_dib_info(dib_info *dib)
{
    if (dib->flags & private_color_table)
        HeapFree(GetProcessHeap(), 0, dib->color_table);
    dib->color_table = NULL;

    HeapFree(GetProcessHeap(), 0, dib->ptr_to_free);
    dib->ptr_to_free = NULL;
    dib->bits = NULL;
}

void copy_dib_color_info(dib_info *dst, const dib_info *src)
{
    dst->bit_count        = src->bit_count;
    dst->red_mask         = src->red_mask;
    dst->green_mask       = src->green_mask;
    dst->blue_mask        = src->blue_mask;
    dst->red_len          = src->red_len;
    dst->green_len        = src->green_len;
    dst->blue_len         = src->blue_len;
    dst->red_shift        = src->red_shift;
    dst->green_shift      = src->green_shift;
    dst->blue_shift       = src->blue_shift;
    dst->funcs            = src->funcs;
    dst->color_table_size = src->color_table_size;
    dst->color_table      = NULL;
    dst->flags            = src->flags;
    if(dst->color_table_size)
    {
        int size = dst->color_table_size * sizeof(dst->color_table[0]);
        if (dst->flags & private_color_table)
        {
            dst->color_table = HeapAlloc(GetProcessHeap(), 0, size);
            memcpy(dst->color_table, src->color_table, size);
        }
        else
            dst->color_table = src->color_table;
    }
}

DWORD convert_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                          const BITMAPINFO *dst_info, void *dst_bits )
{
    dib_info src_dib, dst_dib;
    DWORD ret;

    if ( !init_dib_info_from_bitmapinfo( &src_dib, src_info, src_bits, 0 ) )
        return ERROR_BAD_FORMAT;
    if ( !init_dib_info_from_bitmapinfo( &dst_dib, dst_info, dst_bits, 0 ) )
        return ERROR_BAD_FORMAT;

    ret = dst_dib.funcs->convert_to( &dst_dib, &src_dib, &src->visrect );

    /* We shared the color tables, so there's no need to free the dib_infos here */
    if(!ret) return ERROR_BAD_FORMAT;

    /* update coordinates, the destination rectangle is always stored at 0,0 */
    src->x -= src->visrect.left;
    src->y -= src->visrect.top;
    offset_rect( &src->visrect, -src->visrect.left, -src->visrect.top );
    return ERROR_SUCCESS;
}

static void update_fg_colors( dibdrv_physdev *pdev )
{
    pdev->pen_color   = get_fg_color( pdev, pdev->pen_colorref );
    pdev->brush_color = get_fg_color( pdev, pdev->brush_colorref );
}

static void update_masks( dibdrv_physdev *pdev, INT rop )
{
    calc_and_xor_masks( rop, pdev->pen_color, &pdev->pen_and, &pdev->pen_xor );
    update_brush_rop( pdev, rop );
    if( GetBkMode( pdev->dev.hdc ) == OPAQUE )
        calc_and_xor_masks( rop, pdev->bkgnd_color, &pdev->bkgnd_and, &pdev->bkgnd_xor );
}

 /***********************************************************************
 *           add_extra_clipping_region
 *
 * Temporarily add a region to the current clipping region.
 * The returned region must be restored with restore_clipping_region.
 */
static HRGN add_extra_clipping_region( dibdrv_physdev *pdev, HRGN rgn )
{
    HRGN ret, clip;

    if (!(clip = CreateRectRgn( 0, 0, 0, 0 ))) return 0;
    CombineRgn( clip, pdev->clip, rgn, RGN_AND );
    ret = pdev->clip;
    pdev->clip = clip;
    return ret;
}

/***********************************************************************
 *           restore_clipping_region
 */
static void restore_clipping_region( dibdrv_physdev *pdev, HRGN rgn )
{
    if (!rgn) return;
    DeleteObject( pdev->clip );
    pdev->clip = rgn;
}

/***********************************************************************
 *           dibdrv_DeleteDC
 */
static BOOL dibdrv_DeleteDC( PHYSDEV dev )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    TRACE("(%p)\n", dev);
    DeleteObject(pdev->clip);
    free_pattern_brush(pdev);
    free_dib_info(&pdev->dib);
    return 0;
}

static void set_color_info( const dib_info *dib, BITMAPINFO *info )
{
    DWORD *masks = (DWORD *)info->bmiColors;

    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biClrUsed     = 0;

    switch (info->bmiHeader.biBitCount)
    {
    case 1:
    case 4:
    case 8:
        if (dib->color_table)
        {
            info->bmiHeader.biClrUsed = min( dib->color_table_size, 1 << dib->bit_count );
            memcpy( info->bmiColors, dib->color_table,
                    info->bmiHeader.biClrUsed * sizeof(RGBQUAD) );
        }
        break;
    case 16:
        masks[0] = dib->red_mask;
        masks[1] = dib->green_mask;
        masks[2] = dib->blue_mask;
        info->bmiHeader.biCompression = BI_BITFIELDS;
        break;
    case 32:
        if (dib->funcs != &funcs_8888)
        {
            masks[0] = dib->red_mask;
            masks[1] = dib->green_mask;
            masks[2] = dib->blue_mask;
            info->bmiHeader.biCompression = BI_BITFIELDS;
        }
        break;
    }
}

/***********************************************************************
 *           dibdrv_GetImage
 */
static DWORD dibdrv_GetImage( PHYSDEV dev, HBITMAP hbitmap, BITMAPINFO *info,
                              struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    DWORD ret = ERROR_SUCCESS;
    dib_info *dib, stand_alone;

    TRACE( "%p %p %p\n", dev, hbitmap, info );

    info->bmiHeader.biSize          = sizeof(info->bmiHeader);
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed       = 0;
    info->bmiHeader.biClrImportant  = 0;

    if (hbitmap)
    {
        BITMAPOBJ *bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );

        if (!bmp) return ERROR_INVALID_HANDLE;
        assert(bmp->dib);

        if (!init_dib_info( &stand_alone, &bmp->dib->dsBmih, bmp->dib->dsBitfields,
                            bmp->color_table, bmp->nb_colors, bmp->dib->dsBm.bmBits, 0 ))
        {
            ret = ERROR_BAD_FORMAT;
            goto done;
        }
        dib = &stand_alone;
    }
    else
    {
        dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
        dib = &pdev->dib;
    }

    info->bmiHeader.biWidth     = dib->width;
    info->bmiHeader.biHeight    = dib->stride > 0 ? -dib->height : dib->height;
    info->bmiHeader.biBitCount  = dib->bit_count;
    info->bmiHeader.biSizeImage = dib->height * abs( dib->stride );

    set_color_info( dib, info );

    if (bits)
    {
        bits->ptr = dib->bits;
        if (dib->stride < 0)
            bits->ptr = (char *)bits->ptr + (dib->height - 1) * dib->stride;
        bits->is_copy = FALSE;
        bits->free = NULL;
    }

done:
   if (hbitmap) GDI_ReleaseObj( hbitmap );
   return ret;
}

static BOOL matching_color_info( const dib_info *dib, const BITMAPINFO *info )
{
    switch (info->bmiHeader.biBitCount)
    {
    case 1:
    case 4:
    case 8:
    {
        RGBQUAD *color_table = (RGBQUAD *)((char *)info + info->bmiHeader.biSize);
        if (dib->color_table_size != info->bmiHeader.biClrUsed ) return FALSE;
        return memcmp( color_table, dib->color_table, dib->color_table_size * sizeof(RGBQUAD) );
    }

    case 16:
    {
        DWORD *masks = (DWORD *)info->bmiColors;
        if (info->bmiHeader.biCompression == BI_RGB) return dib->funcs == &funcs_555;
        if (info->bmiHeader.biCompression == BI_BITFIELDS)
            return masks[0] == dib->red_mask && masks[1] == dib->green_mask && masks[2] == dib->blue_mask;
        break;
    }

    case 24:
        return TRUE;

    case 32:
    {
        DWORD *masks = (DWORD *)info->bmiColors;
        if (info->bmiHeader.biCompression == BI_RGB) return dib->funcs == &funcs_8888;
        if (info->bmiHeader.biCompression == BI_BITFIELDS)
            return masks[0] == dib->red_mask && masks[1] == dib->green_mask && masks[2] == dib->blue_mask;
        break;
    }

    }

    return FALSE;
}

static inline BOOL rop_uses_pat(DWORD rop)
{
    return ((rop >> 4) & 0x0f0000) != (rop & 0x0f0000);
}

/***********************************************************************
 *           dibdrv_PutImage
 */
static DWORD dibdrv_PutImage( PHYSDEV dev, HBITMAP hbitmap, HRGN clip, BITMAPINFO *info,
                              const struct gdi_image_bits *bits, struct bitblt_coords *src,
                              struct bitblt_coords *dst, DWORD rop )
{
    dib_info *dib, stand_alone;
    DWORD ret;
    POINT origin;
    dib_info src_dib;
    HRGN total_clip, saved_clip = NULL;
    dibdrv_physdev *pdev = NULL;
    const WINEREGION *clip_data;
    int i, rop2;

    TRACE( "%p %p %p\n", dev, hbitmap, info );

    if (!hbitmap && rop_uses_pat( rop ))
    {
        PHYSDEV next = GET_NEXT_PHYSDEV( dev, pPutImage );
        FIXME( "rop %08x unsupported, forwarding to graphics driver\n", rop );
        return next->funcs->pPutImage( next, 0, clip, info, bits, src, dst, rop );
    }

    if (hbitmap)
    {
        BITMAPOBJ *bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );

        if (!bmp) return ERROR_INVALID_HANDLE;
        assert(bmp->dib);

        if (!init_dib_info( &stand_alone, &bmp->dib->dsBmih, bmp->dib->dsBitfields,
                            bmp->color_table, bmp->nb_colors, bmp->dib->dsBm.bmBits, 0 ))
        {
            ret = ERROR_BAD_FORMAT;
            goto done;
        }
        dib = &stand_alone;
    }
    else
    {
        pdev = get_dibdrv_pdev( dev );
        dib = &pdev->dib;
    }

    if (info->bmiHeader.biPlanes != 1) goto update_format;
    if (info->bmiHeader.biBitCount != dib->bit_count) goto update_format;
    if (!matching_color_info( dib, info )) goto update_format;
    if (!bits)
    {
        ret = ERROR_SUCCESS;
        goto done;
    }
    if ((src->width != dst->width) || (src->height != dst->height))
    {
        ret = ERROR_TRANSFORM_NOT_SUPPORTED;
        goto done;
    }

    init_dib_info_from_bitmapinfo( &src_dib, info, bits->ptr, 0 );

    origin.x = src->visrect.left;
    origin.y = src->visrect.top;

    if (hbitmap)
    {
        total_clip = clip;
        rop2 = R2_COPYPEN;
    }
    else
    {
        if (clip) saved_clip = add_extra_clipping_region( pdev, clip );
        total_clip = pdev->clip;
        rop2 =  ((rop >> 16) & 0xf) + 1;
    }

    if (total_clip == NULL) dib->funcs->copy_rect( dib, &dst->visrect, &src_dib, &origin, rop2 );
    else
    {
        clip_data = get_wine_region( total_clip );
        for (i = 0; i < clip_data->numRects; i++)
        {
            RECT clipped_rect;

            if (intersect_rect( &clipped_rect, &dst->visrect, clip_data->rects + i ))
            {
                origin.x = src->visrect.left + clipped_rect.left - dst->visrect.left;
                origin.y = src->visrect.top + clipped_rect.top  - dst->visrect.top;
                dib->funcs->copy_rect( dib, &clipped_rect, &src_dib, &origin, rop2 );
            }
        }
        release_wine_region( total_clip );
    }
    ret = ERROR_SUCCESS;

    if (saved_clip) restore_clipping_region( pdev, saved_clip );

    goto done;

update_format:
    info->bmiHeader.biPlanes   = 1;
    info->bmiHeader.biBitCount = dib->bit_count;
    set_color_info( dib, info );
    ret = ERROR_BAD_FORMAT;

done:
    if (hbitmap) GDI_ReleaseObj( hbitmap );

    return ret;
}

/***********************************************************************
 *           dibdrv_SelectBitmap
 */
static HBITMAP dibdrv_SelectBitmap( PHYSDEV dev, HBITMAP bitmap )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSelectBitmap );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    BITMAPOBJ *bmp = GDI_GetObjPtr( bitmap, OBJ_BITMAP );
    TRACE("(%p, %p)\n", dev, bitmap);

    if (!bmp) return 0;
    assert(bmp->dib);

    pdev->clip = CreateRectRgn(0, 0, 0, 0);
    pdev->defer = 0;

    clear_dib_info(&pdev->dib);
    clear_dib_info(&pdev->brush_dib);
    pdev->brush_and_bits = pdev->brush_xor_bits = NULL;

    if(!init_dib_info(&pdev->dib, &bmp->dib->dsBmih, bmp->dib->dsBitfields,
                      bmp->color_table, bmp->nb_colors, bmp->dib->dsBm.bmBits, private_color_table))
        pdev->defer |= DEFER_FORMAT;

    GDI_ReleaseObj( bitmap );

    return next->funcs->pSelectBitmap( next, bitmap );
}

/***********************************************************************
 *           dibdrv_SetBkColor
 */
static COLORREF dibdrv_SetBkColor( PHYSDEV dev, COLORREF color )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetBkColor );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    pdev->bkgnd_color = pdev->dib.funcs->colorref_to_pixel( &pdev->dib, color );

    if( GetBkMode(dev->hdc) == OPAQUE )
        calc_and_xor_masks( GetROP2(dev->hdc), pdev->bkgnd_color, &pdev->bkgnd_and, &pdev->bkgnd_xor );
    else
    {
        pdev->bkgnd_and = ~0u;
        pdev->bkgnd_xor = 0;
    }

    update_fg_colors( pdev ); /* Only needed in the 1 bpp case */

    return next->funcs->pSetBkColor( next, color );
}

/***********************************************************************
 *           dibdrv_SetBkMode
 */
static INT dibdrv_SetBkMode( PHYSDEV dev, INT mode )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetBkMode );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if( mode == OPAQUE )
        calc_and_xor_masks( GetROP2(dev->hdc), pdev->bkgnd_color, &pdev->bkgnd_and, &pdev->bkgnd_xor );
    else
    {
        pdev->bkgnd_and = ~0u;
        pdev->bkgnd_xor = 0;
    }

    return next->funcs->pSetBkMode( next, mode );
}

/***********************************************************************
 *           dibdrv_SetDeviceClipping
 */
static void dibdrv_SetDeviceClipping( PHYSDEV dev, HRGN vis_rgn, HRGN clip_rgn )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDeviceClipping );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    TRACE("(%p, %p, %p)\n", dev, vis_rgn, clip_rgn);

    CombineRgn( pdev->clip, vis_rgn, clip_rgn, clip_rgn ? RGN_AND : RGN_COPY );
    return next->funcs->pSetDeviceClipping( next, vis_rgn, clip_rgn);
}

/***********************************************************************
 *           dibdrv_SetDIBColorTable
 */
static UINT dibdrv_SetDIBColorTable( PHYSDEV dev, UINT pos, UINT count, const RGBQUAD *colors )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDIBColorTable );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    TRACE("(%p, %d, %d, %p)\n", dev, pos, count, colors);

    if( pdev->dib.color_table && pos < pdev->dib.color_table_size )
    {
        if( pos + count > pdev->dib.color_table_size ) count = pdev->dib.color_table_size - pos;
        memcpy( pdev->dib.color_table + pos, colors, count * sizeof(RGBQUAD) );

        pdev->bkgnd_color = pdev->dib.funcs->colorref_to_pixel( &pdev->dib, GetBkColor( dev->hdc ) );
        update_fg_colors( pdev );

        update_masks( pdev, GetROP2( dev->hdc ) );
    }
    return next->funcs->pSetDIBColorTable( next, pos, count, colors );
}

/***********************************************************************
 *           dibdrv_SetROP2
 */
static INT dibdrv_SetROP2( PHYSDEV dev, INT rop )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetROP2 );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    update_masks( pdev, rop );

    return next->funcs->pSetROP2( next, rop );
}

const DC_FUNCTIONS dib_driver =
{
    NULL,                               /* pAbortDoc */
    NULL,                               /* pAbortPath */
    NULL,                               /* pAlphaBlend */
    NULL,                               /* pAngleArc */
    NULL,                               /* pArc */
    NULL,                               /* pArcTo */
    NULL,                               /* pBeginPath */
    NULL,                               /* pChoosePixelFormat */
    NULL,                               /* pChord */
    NULL,                               /* pCloseFigure */
    NULL,                               /* pCreateBitmap */
    NULL,                               /* pCreateDC */
    NULL,                               /* pCreateDIBSection */
    NULL,                               /* pDeleteBitmap */
    dibdrv_DeleteDC,                    /* pDeleteDC */
    NULL,                               /* pDeleteObject */
    NULL,                               /* pDescribePixelFormat */
    NULL,                               /* pDeviceCapabilities */
    NULL,                               /* pEllipse */
    NULL,                               /* pEndDoc */
    NULL,                               /* pEndPage */
    NULL,                               /* pEndPath */
    NULL,                               /* pEnumDeviceFonts */
    NULL,                               /* pEnumICMProfiles */
    NULL,                               /* pExcludeClipRect */
    NULL,                               /* pExtDeviceMode */
    NULL,                               /* pExtEscape */
    NULL,                               /* pExtFloodFill */
    NULL,                               /* pExtSelectClipRgn */
    NULL,                               /* pExtTextOut */
    NULL,                               /* pFillPath */
    NULL,                               /* pFillRgn */
    NULL,                               /* pFlattenPath */
    NULL,                               /* pFrameRgn */
    NULL,                               /* pGdiComment */
    NULL,                               /* pGetCharWidth */
    NULL,                               /* pGetDeviceCaps */
    NULL,                               /* pGetDeviceGammaRamp */
    NULL,                               /* pGetICMProfile */
    dibdrv_GetImage,                    /* pGetImage */
    NULL,                               /* pGetNearestColor */
    NULL,                               /* pGetPixel */
    NULL,                               /* pGetPixelFormat */
    NULL,                               /* pGetSystemPaletteEntries */
    NULL,                               /* pGetTextExtentExPoint */
    NULL,                               /* pGetTextMetrics */
    NULL,                               /* pIntersectClipRect */
    NULL,                               /* pInvertRgn */
    dibdrv_LineTo,                      /* pLineTo */
    NULL,                               /* pModifyWorldTransform */
    NULL,                               /* pMoveTo */
    NULL,                               /* pOffsetClipRgn */
    NULL,                               /* pOffsetViewportOrg */
    NULL,                               /* pOffsetWindowOrg */
    dibdrv_PaintRgn,                    /* pPaintRgn */
    dibdrv_PatBlt,                      /* pPatBlt */
    NULL,                               /* pPie */
    NULL,                               /* pPolyBezier */
    NULL,                               /* pPolyBezierTo */
    NULL,                               /* pPolyDraw */
    NULL,                               /* pPolyPolygon */
    NULL,                               /* pPolyPolyline */
    NULL,                               /* pPolygon */
    NULL,                               /* pPolyline */
    NULL,                               /* pPolylineTo */
    dibdrv_PutImage,                    /* pPutImage */
    NULL,                               /* pRealizeDefaultPalette */
    NULL,                               /* pRealizePalette */
    dibdrv_Rectangle,                   /* pRectangle */
    NULL,                               /* pResetDC */
    NULL,                               /* pRestoreDC */
    NULL,                               /* pRoundRect */
    NULL,                               /* pSaveDC */
    NULL,                               /* pScaleViewportExt */
    NULL,                               /* pScaleWindowExt */
    dibdrv_SelectBitmap,                /* pSelectBitmap */
    dibdrv_SelectBrush,                 /* pSelectBrush */
    NULL,                               /* pSelectClipPath */
    NULL,                               /* pSelectFont */
    NULL,                               /* pSelectPalette */
    dibdrv_SelectPen,                   /* pSelectPen */
    NULL,                               /* pSetArcDirection */
    dibdrv_SetBkColor,                  /* pSetBkColor */
    dibdrv_SetBkMode,                   /* pSetBkMode */
    dibdrv_SetDCBrushColor,             /* pSetDCBrushColor */
    dibdrv_SetDCPenColor,               /* pSetDCPenColor */
    dibdrv_SetDIBColorTable,            /* pSetDIBColorTable */
    NULL,                               /* pSetDIBitsToDevice */
    dibdrv_SetDeviceClipping,           /* pSetDeviceClipping */
    NULL,                               /* pSetDeviceGammaRamp */
    NULL,                               /* pSetLayout */
    NULL,                               /* pSetMapMode */
    NULL,                               /* pSetMapperFlags */
    NULL,                               /* pSetPixel */
    NULL,                               /* pSetPixelFormat */
    NULL,                               /* pSetPolyFillMode */
    dibdrv_SetROP2,                     /* pSetROP2 */
    NULL,                               /* pSetRelAbs */
    NULL,                               /* pSetStretchBltMode */
    NULL,                               /* pSetTextAlign */
    NULL,                               /* pSetTextCharacterExtra */
    NULL,                               /* pSetTextColor */
    NULL,                               /* pSetTextJustification */
    NULL,                               /* pSetViewportExt */
    NULL,                               /* pSetViewportOrg */
    NULL,                               /* pSetWindowExt */
    NULL,                               /* pSetWindowOrg */
    NULL,                               /* pSetWorldTransform */
    NULL,                               /* pStartDoc */
    NULL,                               /* pStartPage */
    NULL,                               /* pStretchBlt */
    NULL,                               /* pStretchDIBits */
    NULL,                               /* pStrokeAndFillPath */
    NULL,                               /* pStrokePath */
    NULL,                               /* pSwapBuffers */
    NULL,                               /* pUnrealizePalette */
    NULL,                               /* pWidenPath */
    NULL,                               /* pwglCopyContext */
    NULL,                               /* pwglCreateContext */
    NULL,                               /* pwglCreateContextAttribsARB */
    NULL,                               /* pwglDeleteContext */
    NULL,                               /* pwglGetPbufferDCARB */
    NULL,                               /* pwglGetProcAddress */
    NULL,                               /* pwglMakeContextCurrentARB */
    NULL,                               /* pwglMakeCurrent */
    NULL,                               /* pwglSetPixelFormatWINE */
    NULL,                               /* pwglShareLists */
    NULL,                               /* pwglUseFontBitmapsA */
    NULL                                /* pwglUseFontBitmapsW */
};
