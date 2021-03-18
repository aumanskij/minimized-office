/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_VCL_INC_BITMAP_SCANLINETOOLS_HXX
#define INCLUDED_VCL_INC_BITMAP_SCANLINETOOLS_HXX

#include <tools/color.hxx>
#include <vcl/BitmapPalette.hxx>

namespace vcl::bitmap
{
class IScanlineTransformer
{
public:
    virtual void startLine(sal_uInt8* pLine) = 0;
    virtual void skipPixel(sal_uInt32 nPixel) = 0;
    virtual Color readPixel() = 0;
    virtual void writePixel(Color nColor) = 0;

    virtual ~IScanlineTransformer() = default;
};

class ScanlineTransformer_RGB565 final : public IScanlineTransformer
{
protected:
    sal_uInt16* mpData;

public:
    void startLine(sal_uInt8* pLine) override { mpData = reinterpret_cast<sal_uInt16*>(pLine); }

    void skipPixel(sal_uInt32 nPixel) override { mpData += nPixel; }

    Color readPixel() override
    {
        sal_uInt8 R = sal_uInt8((*mpData & 0xf800) >> 8);
        sal_uInt8 G = sal_uInt8((*mpData & 0x07e0) >> 3);
        sal_uInt8 B = sal_uInt8((*mpData & 0x001f) << 3);
        mpData++;
        return Color(R, G, B);
    }

    void writePixel(Color nColor) override
    {
        sal_uInt16 R = (nColor.GetRed() & 0xf8) << 8;
        sal_uInt16 G = (nColor.GetGreen() & 0xfc) << 3;
        sal_uInt16 B = (nColor.GetBlue() & 0xf8) >> 3;
        *mpData++ = R | G | B;
    }
};

class ScanlineTransformerBase : public IScanlineTransformer
{
protected:
    sal_uInt8* mpData;

public:
    ScanlineTransformerBase()
        : mpData(nullptr)
    {
    }

    void startLine(sal_uInt8* pLine) override { mpData = pLine; }
};

class ScanlineTransformer_ARGB final : public ScanlineTransformerBase
{
public:
    void skipPixel(sal_uInt32 nPixel) override { mpData += nPixel << 2; }

    Color readPixel() override
    {
        const Color aColor(ColorTransparency, mpData[4], mpData[1], mpData[2], mpData[3]);
        mpData += 4;
        return aColor;
    }

    void writePixel(Color nColor) override
    {
        *mpData++ = 255 - nColor.GetAlpha();
        *mpData++ = nColor.GetRed();
        *mpData++ = nColor.GetGreen();
        *mpData++ = nColor.GetBlue();
    }
};

class ScanlineTransformer_BGR final : public ScanlineTransformerBase
{
public:
    void skipPixel(sal_uInt32 nPixel) override { mpData += (nPixel << 1) + nPixel; }

    Color readPixel() override
    {
        const Color aColor(mpData[2], mpData[1], mpData[0]);
        mpData += 3;
        return aColor;
    }

    void writePixel(Color nColor) override
    {
        *mpData++ = nColor.GetBlue();
        *mpData++ = nColor.GetGreen();
        *mpData++ = nColor.GetRed();
    }
};

class ScanlineTransformerPaletteBase : public ScanlineTransformerBase
{
protected:
    const BitmapPalette& mrPalette;

public:
    ScanlineTransformerPaletteBase(const BitmapPalette& rPalette)
        : ScanlineTransformerBase()
        , mrPalette(rPalette)
    {
    }
};

class ScanlineTransformer_8BitPalette final : public ScanlineTransformerPaletteBase
{
public:
    explicit ScanlineTransformer_8BitPalette(const BitmapPalette& rPalette)
        : ScanlineTransformerPaletteBase(rPalette)
    {
    }

    void skipPixel(sal_uInt32 nPixel) override { mpData += nPixel; }

    Color readPixel() override
    {
        const sal_uInt8 nIndex(*mpData++);
        if (nIndex < mrPalette.GetEntryCount())
            return mrPalette[nIndex];
        else
            return COL_BLACK;
    }

    void writePixel(Color nColor) override
    {
        *mpData++ = static_cast<sal_uInt8>(mrPalette.GetBestIndex(nColor));
    }
};

class ScanlineTransformer_4BitPalette final : public ScanlineTransformerPaletteBase
{
private:
    sal_uInt32 mnX;
    sal_uInt32 mnShift;

public:
    explicit ScanlineTransformer_4BitPalette(const BitmapPalette& rPalette)
        : ScanlineTransformerPaletteBase(rPalette)
        , mnX(0)
        , mnShift(0)
    {
    }

    void skipPixel(sal_uInt32 nPixel) override
    {
        mnX += nPixel;
        if (nPixel & 1) // is nPixel an odd number
            mnShift ^= 4;
    }

    void startLine(sal_uInt8* pLine) override
    {
        ScanlineTransformerBase::startLine(pLine);
        mnX = 0;
        mnShift = 4;
    }

    Color readPixel() override
    {
        const sal_uInt32 nDataIndex = mnX / 2;
        const sal_uInt8 nIndex((mpData[nDataIndex] >> mnShift) & 0x0f);
        mnX++;
        mnShift ^= 4;

        if (nIndex < mrPalette.GetEntryCount())
            return mrPalette[nIndex];
        else
            return COL_BLACK;
    }

    void writePixel(Color nColor) override
    {
        const sal_uInt32 nDataIndex = mnX / 2;
        const sal_uInt8 nColorIndex = mrPalette.GetBestIndex(nColor);
        mpData[nDataIndex] |= (nColorIndex & 0x0f) << mnShift;
        mnX++;
        mnShift ^= 4;
    }
};

class ScanlineTransformer_1BitPalette final : public ScanlineTransformerPaletteBase
{
private:
    sal_uInt32 mnX;

public:
    explicit ScanlineTransformer_1BitPalette(const BitmapPalette& rPalette)
        : ScanlineTransformerPaletteBase(rPalette)
        , mnX(0)
    {
    }

    void skipPixel(sal_uInt32 nPixel) override { mnX += nPixel; }

    void startLine(sal_uInt8* pLine) override
    {
        ScanlineTransformerBase::startLine(pLine);
        mnX = 0;
    }

    Color readPixel() override
    {
        const sal_uInt8 nIndex((mpData[mnX >> 3] >> (7 - (mnX & 7))) & 1);
        mnX++;

        if (nIndex < mrPalette.GetEntryCount())
            return mrPalette[nIndex];
        else
            return COL_BLACK;
    }

    void writePixel(Color nColor) override
    {
        if (mrPalette.GetBestIndex(nColor) & 1)
            mpData[mnX >> 3] |= 1 << (7 - (mnX & 7));
        else
            mpData[mnX >> 3] &= ~(1 << (7 - (mnX & 7)));
        mnX++;
    }
};

std::unique_ptr<IScanlineTransformer> getScanlineTransformer(sal_uInt16 nBits,
                                                             const BitmapPalette& rPalette)
{
    switch (nBits)
    {
        case 1:
            return std::make_unique<ScanlineTransformer_1BitPalette>(rPalette);
        case 4:
            return std::make_unique<ScanlineTransformer_4BitPalette>(rPalette);
        case 8:
            return std::make_unique<ScanlineTransformer_8BitPalette>(rPalette);
        case 16:
            return std::make_unique<ScanlineTransformer_RGB565>();
        case 24:
            return std::make_unique<ScanlineTransformer_BGR>();
        case 32:
            return std::make_unique<ScanlineTransformer_ARGB>();
        default:
            assert(false);
            break;
    }
    return nullptr;
}
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
