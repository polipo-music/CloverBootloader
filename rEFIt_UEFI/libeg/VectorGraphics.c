/*
 * Additional procedures for vector theme support
 *
 * Slice, 2018
 *
 */


#include "nanosvg.h"
#include "FloatLib.h"

#include "lodepng.h"

#ifndef DEBUG_ALL
#define DEBUG_VEC 1
#else
#define DEBUG_VEC DEBUG_ALL
#endif

#if DEBUG_VEC == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_VEC, __VA_ARGS__)
#endif

#define TEST_MATH 0
#define TEST_SVG_IMAGE 1
#define TEST_SIZEOF 0
#define TEST_FONT 0

#define NSVG_RGB(r, g, b) (((unsigned int)b) | ((unsigned int)g << 8) | ((unsigned int)r << 16))
//#define NSVG_RGBA(r, g, b, a) (((unsigned int)b) | ((unsigned int)g << 8) | ((unsigned int)r << 16) | ((unsigned int)a << 24))

extern VOID
WaitForKeyPress(CHAR16 *Message);

extern void DumpFloat2 (char* s, float* t, int N);
extern EG_IMAGE *BackgroundImage;
extern EG_IMAGE *Banner;
extern EG_IMAGE *BigBack;
extern VOID *fontsDB;
extern INTN BanHeight;

EG_IMAGE  *ParseSVGIcon(NSVGparser  *p, INTN Id, CHAR8 *IconName, float Scale)
{
//  EFI_STATUS      Status = EFI_NOT_FOUND;
  NSVGimage       *SVGimage;
  NSVGrasterizer* rast = nsvgCreateRasterizer();
  SVGimage = p->image;
  NSVGshape   *shape;
  NSVGgroup   *group;
  NSVGimage *IconImage; // = (NSVGimage*)AllocateZeroPool(sizeof(NSVGimage));
  NSVGshape *shapeNext, *shapesTail=NULL, *shapePrev;

  NSVGparser* p2 = nsvg__createParser();
  IconImage = p2->image;

  shape = SVGimage->shapes;
  shapePrev = NULL;
  while (shape) {
    group = shape->group;

    shapeNext = shape->next;

      while (group) {
        if (strcmp(group->id, IconName) == 0) {
          break;
        }
        group = group->next;
      }

    if (group) { //the shape is in the group
      DBG("found shape %a", shape->id);
      DBG(" from group %a\n", group->id);
      if ((Id == BUILTIN_SELECTION_BIG) ||
          (Id == BUILTIN_ICON_BACKGROUND) ||
          (Id == BUILTIN_ICON_BANNER)) {
        shape->debug = TRUE;
      }
      if (strstr(shape->id, "BoundingRect") != NULL) {
        //there is bounds after nsvgParse()
        IconImage->width = shape->bounds[2] - shape->bounds[0];
        IconImage->height = shape->bounds[3] - shape->bounds[1];

  //      memcpy(IconImage->realBounds, shape->bounds, 4*sizeof(float));
        if (!IconImage->height) {
          IconImage->height = 200;
        }
        if (Id == BUILTIN_ICON_BACKGROUND || Id == BUILTIN_ICON_BANNER) {
          DBG("IconImage size [%d,%d]\n", (int)IconImage->width, (int)IconImage->height);
 //         DBG("IconImage left corner x=%s y=%s\n", PoolPrintFloat(IconImage->realBounds[0]), PoolPrintFloat(IconImage->realBounds[1]));
          DumpFloat2("IconImage real bounds", IconImage->realBounds, 4);
        }
        shape->flags = 0;  //invisible
        if (shapePrev) {
          shapePrev->next = shapeNext;
        } else {
          SVGimage->shapes = shapeNext;
        }
        shape = shapeNext;
//        Status = EFI_SUCCESS;
        continue; //while(shape) it is BoundingRect shape
      }
      shape->flags = NSVG_VIS_VISIBLE;
      DBG("shape opacity=%s, fill color=0x%x\n", PoolPrintFloat(shape->opacity), shape->fill.color);
      //should be added to tail
      // Add to tail
      if (IconImage->shapes == NULL)
        IconImage->shapes = shape;
      else
        shapesTail->next = shape;
      shapesTail = shape;
      if (shapePrev) {
        shapePrev->next = shapeNext;
      } else {
        SVGimage->shapes = shapeNext;
      }
      shapePrev->next = shapeNext;
    } //the shape in the group
    else {
      shapePrev = shape;
    }
    shape = shapeNext;
  } //while shape
  shapesTail->next = NULL;

  //add clipPaths  xxx
  NSVGclipPath* clipPaths = SVGimage->clipPaths;
  NSVGclipPath* clipNext = NULL;
  while (clipPaths) {
    group = clipPaths->shapes->group;
    clipNext = clipPaths->next;
    while (group) {
      if (strcmp(group->id, IconName) == 0) {
        break;
      }
      group = group->parent;
    }
    if (group) {
      DBG("found clipPaths for %a\n", IconName);
      IconImage->clipPaths = clipPaths;
    }
    clipPaths = clipNext;
  }

  float bounds[4];
  bounds[0] = FLT_MAX;
  bounds[1] = FLT_MAX;
  bounds[2] = -FLT_MAX;
  bounds[3] = -FLT_MAX;

  // Guess image size if not set completely.
  nsvg__imageBounds(p2, bounds);
  DumpFloat2("p2 image bounds", bounds, 4);
  // Patch: save real bounds.
  memcpy(IconImage->realBounds, bounds, 4*sizeof(float));


  float Height = IconImage->height * Scale;
  float Width = IconImage->width * Scale;
  DBG("icon %a width=%s height=%s\n", IconName, PoolPrintFloat(Width), PoolPrintFloat(Height));
  EG_IMAGE  *NewImage = NULL;
//  NewImage = egCreateImage((int)Width, (int)Height, TRUE);
  NewImage = egCreateFilledImage((int)Width, (int)Height, TRUE, &DarkSelectionPixel);
  DBG("begin rasterize %a\n", IconName);
  nsvgRasterize(rast, IconImage, 0,0,Scale,Scale, (UINT8*)NewImage->PixelData, (int)Width, (int)Height, (int)Width*4, NULL, NULL);
/*
  BltImageAlpha(NewImage,
                (int)(UGAWidth - NewImage->Width) / 2,
                (int)(UGAHeight * 0.05f),
                &MenuBackgroundPixel,
                16);
//  WaitForKeyPress(L"waiting for key press...\n");
*/
  nsvgDeleteRasterizer(rast);
  nsvg__deleteParser(p2);
  return NewImage;
}

EFI_STATUS ParseSVGTheme(CONST CHAR8* buffer, TagPtr * dict, UINT32 bufSize)
{

  NSVGparser      *p = NULL;
  NSVGfont        *fontSVG;
  NSVGimage       *SVGimage;
  NSVGrasterizer  *rast = nsvgCreateRasterizer();

// --- Parse Theme.svg
  p = nsvgParse((CHAR8*)buffer, "px", 72, 1.f);
  SVGimage = p->image;
  if (!SVGimage) {
    DBG("Theme not parsed!\n");
    return EFI_NOT_STARTED;
  }

// --- Get scale as theme design height vs screen height
  float Scale;
  // must be svg view-box
  float vbx = p->viewWidth;
  float vby = p->viewHeight;
  DBG("Theme view-bounds: w=%d h=%d units=%a\n", (int)vbx, (int)vby, "px");
    if (vby > 1.0f) {
      SVGimage->height = vby;
    } else {
      SVGimage->height = 768.f;  //default height
    }
  Scale = UGAHeight / SVGimage->height;
  DBG("using scale %s\n", PoolPrintFloat(Scale));

// --- Get theme embedded font (already parsed above)
  fontSVG = p->font;
  if (!fontSVG) {
    fontSVG = fontsDB;
    while (fontSVG && !fontSVG->glyphs) {
	    fontSVG = fontSVG->next;
    }
  }
  if (fontSVG) {
    DBG("theme uses font-family=%a\n", fontSVG->fontFamily);
  }


// --- Create rastered font
  if (fontSVG) {
    if (p->font) {
      FontHeight = (int)(p->fontSize * Scale);
      DBG("font height=%d color=%x\n", FontHeight, p->fontColor);
    }
    if (!FontHeight) FontHeight = 16;
    if (fontSVG->fontFamily[0] < 0x30) {
      AsciiStrCpyS(fontSVG->fontFamily, 64, fontSVG->id);
    }
    LoadSVGfont(fontSVG, p->fontColor);
    DBG("font %a parsed\n", fontSVG->fontFamily);
  }

// --- Make background
  BackgroundImage = egCreateFilledImage(UGAWidth, UGAHeight, TRUE, &MenuBackgroundPixel);
  BigBack = ParseSVGIcon(p, BUILTIN_ICON_BACKGROUND, "Background", Scale);
  GlobalConfig.BackgroundScale = imScale;

// --- Make Banner
  Banner = ParseSVGIcon(p, BUILTIN_ICON_BANNER, "Banner", Scale);
  BuiltinIconTable[BUILTIN_ICON_BANNER].Image = Banner;
  BanHeight = (int)(Banner->Height * Scale);
  DBG("parsed banner->width=%d\n", Banner->Width);

// --- Make other icons
#if 1
  INTN i = BUILTIN_ICON_FUNC_ABOUT;
  CHAR8           *IconName;
  while (BuiltinIconTable[i].Path) {
    CHAR16 *IconPath = BuiltinIconTable[i].Path;
    CHAR16 *ptr = StrStr(IconPath, L"\\");
    if (!ptr) {
      ptr = IconPath;
    } else {
      ptr++;
    }
    UINTN Size = StrLen(ptr)+1;
    IconName = AllocateZeroPool(Size);
    UnicodeStrToAsciiStrS(ptr, IconName, Size);
    DBG("search for icon name %a\n", IconName);

    BuiltinIconTable[i].Image = ParseSVGIcon(p, i, IconName, Scale);
    if (!BuiltinIconTable[i].Image) {
      DBG("icon %d not parsed\n", i);
    }
    i++;
    FreePool(IconName);
  }
  i = 0;
  while (OSIconsTable[i].name) {
    DBG("search for %a\n", OSIconsTable[i].name);
    OSIconsTable[i].image = ParseSVGIcon(p, i, OSIconsTable[i].name, Scale);
    if (OSIconsTable[i].image == NULL) {
      DBG("OSicon %d not parsed\n", i);
    }
    i++;
  }
#endif

#if 0 //test banner
  DBG("test banner\n");
  NSVGshape   *shape;
  NSVGgroup   *group;
  NSVGimage *BannerSVG = (NSVGimage*)AllocateZeroPool(sizeof(NSVGimage));
  NSVGshape *shapeNext, *shapePrev = NULL;

  shape = SVGimage->shapes;
  while (shape) {
    group = shape->group;

    shapeNext = shape->next;
    while (group) {
      if (strcmp(group->id, "Banner") == 0) {
        break;
      }
      group = group->parent;
    }

    if (group) {
      DBG("found shape %a", shape->id);
      DBG(" from group %a\n", group->id);
      if (strstr(shape->id, "BoundingRect") != NULL) {
        BannerSVG->width = shape->bounds[2] - shape->bounds[0];
        BannerSVG->height = shape->bounds[3] - shape->bounds[1];
        memcpy(BannerSVG->realBounds, shape->bounds, 4*sizeof(float));
        if (!BannerSVG->height) {
          BannerSVG->height = 200;
        }
        DBG("Banner size [%d,%d]\n", (int)BannerSVG->width, (int)BannerSVG->height);
        DBG("Banner shift dx=%s dy=%s\n", PoolPrintFloat(BannerSVG->realBounds[0]), PoolPrintFloat(BannerSVG->realBounds[1]));
        //     shape->flags = 0;  //invisible
        shape = shapeNext;
        continue;
      }
      shape->next = BannerSVG->shapes; //add to head
      BannerSVG->shapes = shape;
      if (shapePrev) {
        shapePrev->next = shapeNext;
      }
    } else {
      shapePrev = shape;
    }
    shape = shapeNext;
  }

  //suppose Banner will be 2-30% of Height
  float Height = UGAHeight * 0.28f;
  float Scale1 = Height / BannerSVG->height;
  float Width = BannerSVG->width * Scale1;
  EG_IMAGE        *NewImage1;
  // FreePool(FileData);
  // FileData = NULL;
  if (Width > 1.0f) {
    UINT8           *FileData = NULL;
    UINTN           FileDataLength = 0U;

    NewImage1 = egCreateImage((int)Width, (int)Height, TRUE);
    DBG("new banner size=[%d,%d]\n", (int)Width, (int)Height);

    float tx = 0; //-Banner->realBounds[0] * Scale;
    float ty = 0; //-Banner->realBounds[1] * Scale;
    //  DBG("Banner shift by %d, %d\n", (int)tx, (int)ty);

    nsvgRasterize(rast, BannerSVG, tx,ty,Scale1,Scale1, (UINT8*)NewImage1->PixelData, (int)Width, (int)Height, ((int)Width)*4, NULL, NULL);
    //now show it!

    BltImageAlpha(NewImage1,
                  (int)(UGAWidth - Width) / 4,
                  (int)(UGAHeight * 0.05f),
                  &MenuBackgroundPixel,
                  16);
    WaitForKeyPress(L"waiting for key press...\n");
    //save banner as png yyyyy

    EFI_UGA_PIXEL *ImagePNG = (EFI_UGA_PIXEL *)NewImage1->PixelData;
    unsigned lode_return = eglodepng_encode(&FileData, &FileDataLength, (CONST UINT8*)ImagePNG, (UINTN)NewImage1->Width, (UINTN)NewImage1->Height);
    if (!lode_return) {
      egSaveFile(SelfRootDir, L"\\Banner.png", FileData, FileDataLength);
      FreePool(FileData);
      FileData = NULL;
    }
    egFreeImage(NewImage1);

  }
  //  nsvg__deleteParser(p);

#endif

#if 0
  BltImageAlpha(Banner,
                (int)(UGAWidth - Banner->Width) / 4,
                (int)(UGAHeight * 0.05f),
                &MenuBackgroundPixel,
                16);
  WaitForKeyPress(L"waiting for key press...\n");

#endif

  //Test text
#if TEST_FONT
  if (fontSVG) {
    EFI_STATUS Status = EFI_SUCCESS;
    INTN iHeight = 260;
    INTN iWidth = UGAWidth-200;
    DBG("test font: create textbuffer\n");
    EG_IMAGE* TextBufferXY = egCreateFilledImage(iWidth, iHeight, TRUE, &DarkSelectionPixel);
    drawSVGtext(TextBufferXY, fontSVG, L"Clover ready", NSVG_RGBA(150, 150, 150, 255));
    //---------
    //save picture as png yyyyy
    UINT8           *FileData = NULL;
    UINTN           FileDataLength = 0U;
    EFI_UGA_PIXEL *ImagePNG = (EFI_UGA_PIXEL *)TextBufferXY->PixelData;

    unsigned lode_return =
    eglodepng_encode(&FileData, &FileDataLength, (CONST UINT8*)ImagePNG, iWidth, iHeight);
    DBG("  encode %d filelen=%d\n", lode_return, FileDataLength);
    if (!lode_return) {
      Status = egSaveFile(SelfRootDir, L"\\TestTextSVG.png", FileData, FileDataLength);
      DBG("save file status=%r\n", Status);
    } else {
      DBG("wrong encode %d\n", lode_return);
    }
    //----------


    DBG("text ready to blit\n");
    BltImageAlpha(TextBufferXY,
                  (UGAWidth - iWidth) / 2,
                  (UGAHeight - iHeight) / 2,
                  &MenuBackgroundPixel,
                  16);
    egFreeImage(TextBufferXY);
    //    DBG("draw finished\n");
    WaitForKeyPress(L"waiting for key press...\n");
  }
#endif

  if (p) {
    nsvg__deleteParser(p);
    p = NULL;
  }

#if 0 //TEST_FONT
  if (p1) {
    nsvg__deleteParser(p1);
    p1 = NULL;
  }
#endif

  nsvgDeleteRasterizer(rast);

  *dict = AllocatePool(sizeof(TagStruct));
  (*dict)->type = kTagTypeNone;
  GlobalConfig.TypeSVG = TRUE;
  GlobalConfig.ThemeDesignHeight = (int)SVGimage->height;
  GlobalConfig.ThemeDesignWidth = (int)SVGimage->width;
  return EFI_SUCCESS;
//  return EFI_NOT_READY;
}

VOID LoadSVGfont(NSVGfont  *fontSVG, UINT32 color)
{
//  EFI_STATUS      Status;
  float           FontScale;
  NSVGparser      *p;
  NSVGrasterizer  *rast;
  INTN i;
  if (!fontSVG) {
    return;
  }
  //free old font
  if (FontImage != NULL) {
    egFreeImage (FontImage);
    FontImage = NULL;
  }
  INTN Height = FontHeight + 2;
  INTN Width = Height * (0xC0 + GlobalConfig.CodepageSize);
  FontImage = egCreateImage(Width, Height, TRUE);
//  DBG("load font %a\n", fontSVG->fontFamily);
  if (fontSVG->unitsPerEm < 1.f) {
    fontSVG->unitsPerEm = 1000.f;
  }
  float fH = fontSVG->bbox[3] - fontSVG->bbox[1];
  if (fH == 0.f) {
    fH = fontSVG->unitsPerEm;
  }
  FontScale = (float)FontHeight / fH;
  DBG("font scale %s\n", PoolPrintFloat(FontScale));

  p = nsvg__createParser();
  if (!p) {
    return;
  }
//  p->font = fontSVG;
  p->image->height = (float)Height;
  p->image->width = Width;

  NSVGtext* text = (NSVGtext*)AllocateZeroPool(sizeof(NSVGtext));
  if (!text) {
    return;
  }
  text->fontSize = (float)FontHeight;
  text->font = fontSVG;
  text->fontColor = color;

//  DBG("LoadSVGfont: fontID=%a\n", text->font->id);
//  DBG("LoadSVGfont:  family=%a\n", text->font->fontFamily);
  //add to head
  text->next = p->text;
  p->text = text;
  //for each letter rasterize glyph into FontImage
  //0..0xC0 == AsciiPageSize
  // cyrillic 0x410..0x450 at 0xC0
  float x = 0.f;
  float y = 0.f;
  p->isText = TRUE;
  for (i = 0; i < AsciiPageSize; i++) {
//    if (i == 'C') {
//      DBG("addLetter %x\n", i);
//    }
    addLetter(p, i, x, y, FontScale, color);
    x += (float)Height;
  }
  x = AsciiPageSize * Height;
  for (i = GlobalConfig.Codepage; i < GlobalConfig.Codepage+GlobalConfig.CodepageSize; i++) {
    addLetter(p, i, x, y, FontScale, color);
    x += (float)Height;
  }
  p->image->realBounds[0] = fontSVG->bbox[0] * FontScale;
  p->image->realBounds[1] = fontSVG->bbox[1] * FontScale;
  p->image->realBounds[2] = fontSVG->bbox[2] * FontScale + x; //last bound
  p->image->realBounds[3] = fontSVG->bbox[3] * FontScale;

  //We made an image, then rasterize it
  rast = nsvgCreateRasterizer();
//  DBG("begin raster text\n");
  nsvgRasterize(rast, p->image, 0, 0, 1.0f, 1.0f, (UINT8*)FontImage->PixelData, (int)Width, (int)Height, (int)(Width*4), NULL, NULL);
//  DBG("end raster text\n");
#if 0 //DEBUG_FONT
  //save font as png yyyyy
  UINT8           *FileData = NULL;
  UINTN           FileDataLength = 0U;

  EFI_UGA_PIXEL *ImagePNG = (EFI_UGA_PIXEL *)FontImage->PixelData;

  unsigned lode_return =
    eglodepng_encode(&FileData, &FileDataLength, (CONST UINT8*)ImagePNG, (UINTN)FontImage->Width, (UINTN)FontImage->Height);

  if (!lode_return) {
    egSaveFile(SelfRootDir, L"\\FontSVG.png", FileData, FileDataLength);
  }
#endif
  nsvgDeleteRasterizer(rast);
//  nsvg__deleteParser(p);
  return;
}
// it is for test purpose

VOID drawSVGtext(EG_IMAGE* TextBufferXY, VOID* font, CONST CHAR16* text, UINT32 color)
{
  INTN Width, Height;
  int i;
  UINTN len;
  NSVGparser* p;
  NSVGshape *shape; //, *cur, *prev;
  NSVGrasterizer* rast;
  NSVGfont* fontSVG = (NSVGfont*)font;
  float Scale, sx, sy;
  float x, y;
  if (!fontSVG) {
    DBG("no font in drawSVGtext\n");
    return;
  }
  if (!TextBufferXY) {
    DBG("no buffer\n");
    return;
  }
  p = nsvg__createParser();
  if (!p) {
    return;
  }
  len = StrLen(text);
  Width = TextBufferXY->Width;
  Height = TextBufferXY->Height;
//  Height = 180; //for test
  DBG("textBuffer: [%d,%d], fontUnits=%d\n", Width, TextBufferXY->Height, (int)fontSVG->unitsPerEm);
  if (!fontSVG->unitsPerEm) {
    fontSVG->unitsPerEm = 1000.f;
  }
  float fH = fontSVG->bbox[3] - fontSVG->bbox[1]; //1250
  if (fH == 0.f) {
    fH = fontSVG->unitsPerEm;  //1000
  }
  sy = (float)Height / fH; //(float)fontSVG->unitsPerEm; // 260./1250.
  //in font units
  float fW = fontSVG->bbox[2] - fontSVG->bbox[0];
  sx = (float)Width / (fW * len);
  Scale = (sx > sy)?sy:sx;
  x = 0.f;
  y = 0.f;
  for (i=0; i < len; i++) {
    CHAR16 letter = text[i];
    NSVGglyph* g;
    if (!letter) {
      break;
    }

    shape = (NSVGshape*)AllocateZeroPool(sizeof(NSVGshape));
    if (shape == NULL) return;
    shape->strokeWidth = 1.2f;

    g = fontSVG->glyphs;
    while (g) {
      if (g->unicode == letter) {
        shape->paths = g->path;
        if (shape->paths) {
          if (letter == L'C') {
            DBG("Found letter %x, point[0]=(%d,%d)\n", letter,
                (int)shape->paths->pts[0], (int)shape->paths->pts[1]);
            shape->debug = TRUE;
          }

        } else {
          DBG("Found letter %x, no path\n", letter);
        }
        break;
      }
      g = g->next;
    }
    if (!g) {
      //missing glyph
      NSVGglyph* g = fontSVG->missingGlyph;
      shape->paths = g->path;
      DBG("Missing letter %x\n", letter);
    }
    if (!shape->paths) {
      if (g) {
        x += g->horizAdvX  * Scale;
      }
      if (shape) {
        FreePool(shape);
      }
      continue;
    }
    //fill shape
    shape->id[0] = (char)(letter & 0xff);
    shape->id[1] = (char)((letter >> 8) & 0xff);
    shape->fill.type = NSVG_PAINT_COLOR;
    shape->fill.color = color; //NSVG_RGBA(150, 150, 150, 255); //dark grey 20%
    shape->stroke.type = NSVG_PAINT_NONE;
    shape->stroke.color = NSVG_RGBA(0,0,0, 255); //black?
    shape->strokeWidth = 2.0f;
    shape->flags = NSVG_VIS_VISIBLE;
    nsvg__xformIdentity(shape->xform);
    shape->xform[0] = 1.f * Scale;
    shape->xform[3] = -1.f * Scale; //glyphs are mirrored by Y
    shape->xform[4] = x - fontSVG->bbox[0] * Scale;
    shape->xform[5] = y + fontSVG->bbox[3] * Scale; // Y2 is a floor for a letter
//    DumpFloat2(shape->xform, 6);
    //in glyph units
    shape->bounds[0] = fontSVG->bbox[0];
    shape->bounds[1] = fontSVG->bbox[1];
    shape->bounds[2] = fontSVG->bbox[2];
    shape->bounds[3] = fontSVG->bbox[3];
    DumpFloat2("letter bounds", shape->bounds, 4);

    x += g->horizAdvX * Scale; //position for next letter
    shape->strokeLineJoin = NSVG_JOIN_MITER;
    shape->strokeLineCap = NSVG_CAP_BUTT;
    shape->miterLimit = 4;
    shape->fillRule = NSVG_FILLRULE_NONZERO;
    shape->opacity = 1.f;

    // Add to tail
    if (p->image->shapes == NULL)
      p->image->shapes = shape;
    else
      p->shapesTail->next = shape;
    p->shapesTail = shape;
  } //end of text

  p->image->realBounds[0] = fontSVG->bbox[0] * Scale;
  p->image->realBounds[1] = fontSVG->bbox[1] * Scale;
  p->image->realBounds[2] = fontSVG->bbox[2] * Scale + x; //last bound
  p->image->realBounds[3] = fontSVG->bbox[3] * Scale;

  //We made an image, then rasterize it
  rast = nsvgCreateRasterizer();
  DBG("begin raster text, scale=%s\n", PoolPrintFloat(Scale));
  nsvgRasterize(rast, p->image, 0, 0, 1.f, 1.f, (UINT8*)TextBufferXY->PixelData, (int)Width, (int)Height, (int)(Width*4), NULL, NULL);

  DBG("end raster text\n");
  nsvgDeleteRasterizer(rast);
  nsvg__deleteParser(p);
}

VOID testSVG()
{
  do {

    EFI_STATUS      Status;
    UINT8           *FileData = NULL;
    UINTN           FileDataLength = 0;

    INTN Width = 400, Height = 400;

#if TEST_MATH
    //Test mathematique
//#define fabsf(x) ((x >= 0.0f)?x:(-x))
#define pr(x) (int)fabsf(x), (int)fabsf((x - (int)x) * 1000000.0f)
    int i;
    float x, y1, y2;
    //          CHAR8 Str[128];
    DBG("Test float: -%d.%06d\n", pr(-0.7612f));
    for (i=0; i<15; i++) {
      x=(PI)/30.0f*i;
      y1=SinF(x);
      y2=CosF(x);

      DBG("x=%d: %d.%06d ", i*6, pr(x));
      DBG("  sinx=%c%d.%06d", (y1<0)?'-':' ', pr(y1));
      DBG("  cosx=%c%d.%06d\n", (y2<0)?'-':' ',pr(y2));
      y1 = Atan2F(y1, y2);
      DBG("  atan2x=%c%d.%06d", (y1<0)?'-':' ',pr(y1));
      y1 = AcosF(y2);
      DBG("  acos=%c%d.%06d", (y1<0)?'-':' ',pr(y1));
      y1 = SqrtF(x);
      DBG("  sqrt=%d.%06d", pr(y1));
      y1 = CeilF(x);
      DBG("  ceil=%c%d.%06d\n", (y1<0)?'-':' ',pr(y1));
    }
#undef pr
#endif
    NSVGparser* p;
#if TEST_SVG_IMAGE
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    EG_IMAGE        *NewImage;
    NSVGimage       *SVGimage;
    float Scale, ScaleX, ScaleY;
    
    // load file
    Status = egLoadFile(SelfRootDir, L"Sample.svg", &FileData, &FileDataLength);
    if (!EFI_ERROR(Status)) {
      //Parse XML to vector data

      p = nsvgParse((CHAR8*)FileData, "px", 72, 0.f);
      SVGimage = p->image;
      DBG("Test image width=%d heigth=%d\n", (int)(SVGimage->width), (int)(SVGimage->height));
      FreePool(FileData);

      // Rasterize
      NewImage = egCreateImage(Width, Height, TRUE);
      if (SVGimage->width <= 0) SVGimage->width = Width;
      if (SVGimage->height <= 0) SVGimage->height = Height;

      ScaleX = Width / SVGimage->width;
      ScaleY = Height / SVGimage->height;
      Scale = (ScaleX > ScaleY)?ScaleY:ScaleX;
      float tx = 0; //-SVGimage->realBounds[0] * Scale;
      float ty = 0; //-SVGimage->realBounds[1] * Scale;
      DBG("timing rasterize start tx=%s ty=%s\n", PoolPrintFloat(tx), PoolPrintFloat(ty));
      nsvgRasterize(rast, SVGimage, tx,ty,Scale,Scale, (UINT8*)NewImage->PixelData, (int)Width, (int)Height, (int)Width*4, NULL, NULL);
      DBG("timing rasterize end\n");
      //now show it!
      BltImageAlpha(NewImage,
                    (UGAWidth - Width) / 2,
                    (UGAHeight - Height) / 2,
                    &MenuBackgroundPixel,
                    16);
      egFreeImage(NewImage);
      nsvg__deleteParser(p);
      nsvgDeleteRasterizer(rast);
    }
#endif
    //Test text
    Height = 260;
    Width = UGAWidth-200;
    DBG("create test textbuffer\n");
    EG_IMAGE* TextBufferXY = egCreateFilledImage(Width, Height, TRUE, &MenuBackgroundPixel);
    Status = egLoadFile(SelfRootDir, L"Font.svg", &FileData, &FileDataLength);
    DBG("test font loaded status=%r\n", Status);
    if (!EFI_ERROR(Status)) {
      p = nsvgParse((CHAR8*)FileData, "px", 72, 1.f);
      if (!p) {
        DBG("font not parsed\n");
        break;
      }
      NSVGfont* fontSVG = p->font;
      DBG("font parsed\n");
      FreePool(FileData);
      //   Scale = Height / fontSVG->unitsPerEm;
      drawSVGtext(TextBufferXY, fontSVG, L"Clover", NSVG_RGBA(0, 0, 0, 255));
      DBG("text ready to blit\n");
      BltImageAlpha(TextBufferXY,
                    (UGAWidth - Width) / 2,
                    (UGAHeight - Height) / 2,
                    &MenuBackgroundPixel,
                    16);
      egFreeImage(TextBufferXY);
      nsvg__deleteParser(p);
      DBG("draw finished\n");
    }
  } while (0);

}