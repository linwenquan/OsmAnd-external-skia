/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkScalerContext_DEFINED
#define SkScalerContext_DEFINED

#include "SkMask.h"
#include "SkMaskGamma.h"
#include "SkMatrix.h"
#include "SkPaint.h"

#ifdef SK_BUILD_FOR_ANDROID
    //For SkFontID
    #include "SkTypeface.h"
#endif

struct SkGlyph;
class SkDescriptor;
class SkMaskFilter;
class SkPathEffect;
class SkRasterizer;

//The following typedef hides from the rest of the implementation the number of
//most significant bits to consider when creating mask gamma tables. Two bits
//per channel was chosen as a balance between fidelity (more bits) and cache
//sizes (fewer bits).
typedef SkTMaskGamma<2, 2, 2> SkMaskGamma;

class SkScalerContext {
public:
    enum Flags {
        kFrameAndFill_Flag        = 0x0001,
        kDevKernText_Flag         = 0x0002,
        kEmbeddedBitmapText_Flag  = 0x0004,
        kEmbolden_Flag            = 0x0008,
        kSubpixelPositioning_Flag = 0x0010,
        kAutohinting_Flag         = 0x0020,
        kVertical_Flag            = 0x0040,

        // together, these two flags resulting in a two bit value which matches
        // up with the SkPaint::Hinting enum.
        kHinting_Shift            = 7, // to shift into the other flags above
        kHintingBit1_Flag         = 0x0080,
        kHintingBit2_Flag         = 0x0100,

        // these should only ever be set if fMaskFormat is LCD16 or LCD32
        kLCD_Vertical_Flag        = 0x0200,    // else Horizontal
        kLCD_BGROrder_Flag        = 0x0400,    // else RGB order

        // Generate A8 from LCD source (for GDI), only meaningful if fMaskFormat is kA8
        // Perhaps we can store this (instead) in fMaskFormat, in hight bit?
        kGenA8FromLCD_Flag        = 0x0800,
    };
    
    // computed values
    enum {
        kHinting_Mask   = kHintingBit1_Flag | kHintingBit2_Flag,
    };

    struct Rec {
        uint32_t    fOrigFontID;
        uint32_t    fFontID;
        SkScalar    fTextSize, fPreScaleX, fPreSkewX;
        SkScalar    fPost2x2[2][2];
        SkScalar    fFrameWidth, fMiterLimit;

        //These describe the parameters to create (uniquely identify) the pre-blend.
        uint32_t    fLumBits;
        uint8_t     fDeviceGamma; //2.6, (0.0, 4.0) gamma, 0.0 for sRGB
        uint8_t     fPaintGamma;  //2.6, (0.0, 4.0) gamma, 0.0 for sRGB
        uint8_t     fContrast;    //0.8+1, [0.0, 1.0] artificial contrast
        uint8_t     fReservedAlign;

        SkScalar getDeviceGamma() const {
            return SkIntToScalar(fDeviceGamma) / (1 << 6);
        }
        void setDeviceGamma(SkScalar dg) {
            SkASSERT(0 <= dg && dg < SkIntToScalar(4));
            fDeviceGamma = SkScalarFloorToInt(dg * (1 << 6));
        }

        SkScalar getPaintGamma() const {
            return SkIntToScalar(fPaintGamma) / (1 << 6);
        }
        void setPaintGamma(SkScalar pg) {
            SkASSERT(0 <= pg && pg < SkIntToScalar(4));
            fPaintGamma = SkScalarFloorToInt(pg * (1 << 6));
        }

        SkScalar getContrast() const {
            return SkIntToScalar(fContrast) / ((1 << 8) - 1);
        }
        void setContrast(SkScalar c) {
            SkASSERT(0 <= c && c <= SK_Scalar1);
            fContrast = SkScalarRoundToInt(c * ((1 << 8) - 1));
        }

        /**
         *  Causes the luminance color and contrast to be ignored, and the
         *  paint and device gamma to be effectively 1.0.
         */
        void ignorePreBlend() {
            setLuminanceColor(0x00000000);
            setPaintGamma(SK_Scalar1);
            setDeviceGamma(SK_Scalar1);
            setContrast(0);
        }

        uint8_t     fMaskFormat;
        uint8_t     fStrokeJoin;
        uint16_t    fFlags;
        // Warning: when adding members note that the size of this structure
        // must be a multiple of 4. SkDescriptor requires that its arguments be
        // multiples of four and this structure is put in an SkDescriptor in
        // SkPaint::MakeRec.

        void    getMatrixFrom2x2(SkMatrix*) const;
        void    getLocalMatrix(SkMatrix*) const;
        void    getSingleMatrix(SkMatrix*) const;

        SkPaint::Hinting getHinting() const {
            unsigned hint = (fFlags & kHinting_Mask) >> kHinting_Shift;
            return static_cast<SkPaint::Hinting>(hint);
        }

        void setHinting(SkPaint::Hinting hinting) {
            fFlags = (fFlags & ~kHinting_Mask) | (hinting << kHinting_Shift);
        }
        
        SkMask::Format getFormat() const {
            return static_cast<SkMask::Format>(fMaskFormat);
        }

        SkColor getLuminanceColor() const {
            return fLumBits;
        }
        
        void setLuminanceColor(SkColor c) {
            fLumBits = c;
        }
    };

    SkScalerContext(const SkDescriptor* desc);
    virtual ~SkScalerContext();

    SkMask::Format getMaskFormat() const {
        return (SkMask::Format)fRec.fMaskFormat;
    }

    bool isSubpixel() const {
        return SkToBool(fRec.fFlags & kSubpixelPositioning_Flag);
    }
    
    // remember our glyph offset/base
    void setBaseGlyphCount(unsigned baseGlyphCount) {
        fBaseGlyphCount = baseGlyphCount;
    }

    /** Return the corresponding glyph for the specified unichar. Since contexts
        may be chained (under the hood), the glyphID that is returned may in
        fact correspond to a different font/context. In that case, we use the
        base-glyph-count to know how to translate back into local glyph space.
     */
    uint16_t charToGlyphID(SkUnichar uni);

    /** Map the glyphID to its glyph index, and then to its char code. Unmapped
        glyphs return zero.
    */
    SkUnichar glyphIDToChar(uint16_t glyphID);

    unsigned    getGlyphCount() { return this->generateGlyphCount(); }
    void        getAdvance(SkGlyph*);
    void        getMetrics(SkGlyph*);
    void        getImage(const SkGlyph&);
    void        getPath(const SkGlyph&, SkPath*);
    void        getFontMetrics(SkPaint::FontMetrics* mX,
                               SkPaint::FontMetrics* mY);

#ifdef SK_BUILD_FOR_ANDROID
    unsigned getBaseGlyphCount(SkUnichar charCode);

    // This function must be public for SkTypeface_android.h, but should not be
    // called by other callers
    SkFontID findTypefaceIdForChar(SkUnichar uni) {
        SkScalerContext* ctx = this;
        while (NULL != ctx) {
            if (ctx->generateCharToGlyph(uni)) {
                return ctx->fRec.fFontID;
            }
            ctx = ctx->getNextContext();
        }
        return 0;
    }
#endif

    static inline void MakeRec(const SkPaint&, const SkMatrix*, Rec* rec);
    static inline void PostMakeRec(const SkPaint&, Rec*);

    static SkScalerContext* Create(const SkDescriptor*);
    static SkMaskGamma::PreBlend GetMaskPreBlend(const Rec& rec);

protected:
    Rec         fRec;
    unsigned    fBaseGlyphCount;

    virtual unsigned generateGlyphCount() = 0;
    virtual uint16_t generateCharToGlyph(SkUnichar) = 0;
    virtual void generateAdvance(SkGlyph*) = 0;
    virtual void generateMetrics(SkGlyph*) = 0;
    virtual void generateImage(const SkGlyph&, SkMaskGamma::PreBlend* maskPreBlend) = 0;
    virtual void generatePath(const SkGlyph&, SkPath*) = 0;
    virtual void generateFontMetrics(SkPaint::FontMetrics* mX,
                                     SkPaint::FontMetrics* mY) = 0;
    // default impl returns 0, indicating failure.
    virtual SkUnichar generateGlyphToChar(uint16_t);

    void forceGenerateImageFromPath() { fGenerateImageFromPath = true; }

private:
    SkPathEffect*   fPathEffect;
    SkMaskFilter*   fMaskFilter;
    SkRasterizer*   fRasterizer;

    // if this is set, we draw the image from a path, rather than
    // calling generateImage.
    bool fGenerateImageFromPath;

    void internalGetPath(const SkGlyph& glyph, SkPath* fillPath,
                         SkPath* devPath, SkMatrix* fillToDevMatrix);

    // return the next context, treating fNextContext as a cache of the answer
    SkScalerContext* getNextContext();

    // returns the right context from our link-list for this glyph. If no match
    // is found, just returns the original context (this)
    SkScalerContext* getGlyphContext(const SkGlyph& glyph);

    // link-list of context, to handle missing chars. null-terminated.
    SkScalerContext* fNextContext;

    // converts linear masks to gamma correcting masks.
    SkMaskGamma::PreBlend fMaskPreBlend;
};

#define kRec_SkDescriptorTag            SkSetFourByteTag('s', 'r', 'e', 'c')
#define kPathEffect_SkDescriptorTag     SkSetFourByteTag('p', 't', 'h', 'e')
#define kMaskFilter_SkDescriptorTag     SkSetFourByteTag('m', 's', 'k', 'f')
#define kRasterizer_SkDescriptorTag     SkSetFourByteTag('r', 'a', 's', 't')

///////////////////////////////////////////////////////////////////////////////

enum SkAxisAlignment {
    kNone_SkAxisAlignment,
    kX_SkAxisAlignment,
    kY_SkAxisAlignment
};

/**
 *  Return the axis (if any) that the baseline for horizontal text will land on
 *  after running through the specified matrix.
 *
 *  As an example, the identity matrix will return kX_SkAxisAlignment
 */
SkAxisAlignment SkComputeAxisAlignmentForHText(const SkMatrix& matrix);

#endif

