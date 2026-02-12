/*
  Read truetype(.ttf) from SD and generate bitmap.

  TrueType™ Reference Manual
  https://developer.apple.com/fonts/TrueType-Reference-Manual/
*/

#include "truetype.h"

truetypeClass ttf;
/* constructor */

truetypeClass::truetypeClass() {}

/* ----------------public---------------- */

void truetypeClass::end()
{
  file.close();
  this->freePointsAll();
  this->freeGlyph();
}

/* initialize */
uint8_t truetypeClass::setTtfFile(File _file, uint8_t _checkCheckSum)
{
  if (_file == 0)
  {
    return 0;
  }

  this->file = _file;
  if (this->readTableDirectory(_checkCheckSum) == 0)
  {
    file.close();
    return 0;
  }

  if (this->readCmap() == 0)
  {
    file.close();
    return 0;
  }

  if (this->readHMetric() == 0)
  {
    file.close();
    return 0;
  }

  this->readKern();
  this->readHeadTable();
  return 1;
}

void truetypeClass::setFramebuffer(uint16_t _framebufferWidth, uint16_t _framebufferHeight, uint8_t _drawbit)
{
  this->displayWidth = _framebufferWidth;
  this->displayHeight = _framebufferHeight;
  this->drawBit = _drawbit;
}

void truetypeClass::setCharacterSize(uint16_t _characterSize)
{
  this->characterSize = _characterSize;
}

void truetypeClass::setCharacterSpacing(int16_t _characterSpace, uint8_t _kerning)
{
  this->characterSpace = _characterSpace;
  this->kerningOn = _kerning;
}

void truetypeClass::setTextBoundary(uint16_t _start_x, uint16_t _end_x, uint16_t _end_y)
{
  this->start_x = _start_x;
  this->end_x = _end_x;
  this->end_y = _end_y;
}

void truetypeClass::setTextColor(uint16_t _onLine, uint16_t _inside)
{
  this->colorLine = _onLine;
  this->colorInside = _inside;
}

void truetypeClass::setTextRotation(uint16_t _rotation)
{
  switch (_rotation)
  {
  case ROTATE_90:
  case 90:
    _rotation = 1;
    break;
  case ROTATE_180:
  case 180:
    _rotation = 2;
    break;
  case ROTATE_270:
  case 270:
    _rotation = 3;
    break;
  default:
    _rotation = 0;
    break;
  }
  this->stringRotation = _rotation;
}

/* ----------------private---------------- */
/* calculate checksum */
uint32_t truetypeClass::calculateCheckSum(uint32_t offset, uint32_t length)
{
  uint32_t checksum = 0L;

  length = (length + 3) / 4;
  file.seek(offset);

  while (length-- > 0)
  {
    checksum += this->getUInt32t();
  }
  return checksum;
}

/* read table directory */
int truetypeClass::readTableDirectory(int checkCheckSum)
{
  file.seek(numTablesPos);
  numTables = this->getUInt16t();
  this->table = (ttTable_t *)malloc(sizeof(ttTable_t) * numTables);

  file.seek(tablePos);
  // Serial0.println("---table list---");
  for (int i = 0; i < numTables; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      this->table[i].name[j] = this->getUInt8t();
      // Serial0.printf("%c", table[i].name[j]);
    }
    this->table[i].name[4] = '\0';
    this->table[i].checkSum = this->getUInt32t();
    this->table[i].offset = this->getUInt32t();
    this->table[i].length = this->getUInt32t();

    // Serial0.printf("--%X", table[i].offset);
    // Serial0.println();
  }

  if (checkCheckSum)
  {
    for (int i = 0; i < numTables; i++)
    {
      if (strcmp(this->table[i].name, "head") != 0)
      { /* checksum of "head" is invalid */
        uint32_t c = this->calculateCheckSum(this->table[i].offset, this->table[i].length);
        if (this->table[i].checkSum != c)
        {
          return 0;
        }
      }
    }
  }
  return 1;
}

/* read head table */
void truetypeClass::readHeadTable()
{
  for (int i = 0; i < numTables; i++)
  {
    if (strcmp(table[i].name, "head") == 0)
    {
      file.seek(table[i].offset);

      headTable.version = this->getUInt32t();
      headTable.revision = this->getUInt32t();
      headTable.checkSumAdjustment = this->getUInt32t();
      headTable.magicNumber = this->getUInt32t();
      headTable.flags = this->getUInt16t();
      headTable.unitsPerEm = this->getUInt16t();
      for (int j = 0; j < 8; j++)
      {
        headTable.created[i] = this->getUInt8t();
      }
      for (int j = 0; j < 8; j++)
      {
        headTable.modified[i] = this->getUInt8t();
      }
      xMin = headTable.xMin = this->getInt16t();
      yMin = headTable.yMin = this->getInt16t();
      xMax = headTable.xMax = this->getInt16t();
      yMax = headTable.yMax = this->getInt16t();
      headTable.macStyle = this->getUInt16t();
      headTable.lowestRecPPEM = this->getUInt16t();
      headTable.fontDirectionHint = this->getInt16t();
      headTable.indexToLocFormat = this->getInt16t();
      headTable.glyphDataFormat = this->getInt16t();
    }
  }
}

/* cmap */
/* read cmap format 4 */
uint8_t truetypeClass::readCmapFormat4()
{
  file.seek(cmapFormat4.offset);
  if ((cmapFormat4.format = this->getUInt16t()) != 4)
  {
    return 0;
  }

  cmapFormat4.length = this->getUInt16t();
  cmapFormat4.language = this->getUInt16t();
  cmapFormat4.segCountX2 = this->getUInt16t();
  cmapFormat4.searchRange = this->getUInt16t();
  cmapFormat4.entrySelector = this->getUInt16t();
  cmapFormat4.rangeShift = this->getUInt16t();
  cmapFormat4.endCodeOffset = cmapFormat4.offset + 14;
  cmapFormat4.startCodeOffset = cmapFormat4.endCodeOffset + cmapFormat4.segCountX2 + 2;
  cmapFormat4.idDeltaOffset = cmapFormat4.startCodeOffset + cmapFormat4.segCountX2;
  cmapFormat4.idRangeOffsetOffset = cmapFormat4.idDeltaOffset + cmapFormat4.segCountX2;
  cmapFormat4.glyphIndexArrayOffset = cmapFormat4.idRangeOffsetOffset + cmapFormat4.segCountX2;

  return 1;
}

/* read cmap */
uint8_t truetypeClass::readCmap()
{
  uint16_t platformId, platformSpecificId;
  uint32_t cmapOffset, tableOffset;
  uint8_t foundMap = 0;

  if ((cmapOffset = this->seekToTable("cmap")) == 0)
  {
    return 0;
  }

  cmapIndex.version = this->getUInt16t();
  cmapIndex.numberSubtables = this->getUInt16t();

  for (uint16_t i = 0; i < cmapIndex.numberSubtables; i++)
  {
    platformId = this->getUInt16t();
    platformSpecificId = this->getUInt16t();
    tableOffset = this->getUInt32t();
    if ((platformId == 3) && (platformSpecificId == 1))
    {
      cmapFormat4.offset = cmapOffset + tableOffset;
      this->readCmapFormat4();
      foundMap = 1;
      break;
    }
  }

  if (foundMap == 0)
  {
    return 0;
  }

  return 1;
}

/* convert character code to glyph id */
uint16_t truetypeClass::codeToGlyphId(uint16_t _code)
{
  uint16_t start, end, idRangeOffset;
  int16_t idDelta;
  uint8_t found = 0;
  uint16_t offset, glyphId;

  for (int i = 0; i < cmapFormat4.segCountX2 / 2; i++)
  {
    file.seek(cmapFormat4.endCodeOffset + 2 * i);
    end = this->getUInt16t();
    if (_code <= end)
    {
      file.seek(cmapFormat4.startCodeOffset + 2 * i);
      start = this->getUInt16t();
      if (_code >= start)
      {
        file.seek(cmapFormat4.idDeltaOffset + 2 * i);
        idDelta = this->getInt16t();
        file.seek(cmapFormat4.idRangeOffsetOffset + 2 * i);
        idRangeOffset = this->getUInt16t();
        if (idRangeOffset == 0)
        {
          glyphId = (idDelta + _code) % 65536;
        }
        else
        {
          offset = (idRangeOffset / 2 + i + _code - start - cmapFormat4.segCountX2 / 2) * 2;
          file.seek(cmapFormat4.glyphIndexArrayOffset + offset);
          glyphId = this->getUInt16t();
        }

        found = 1;
        break;
      }
    }
  }
  if (!found)
  {
    return 0;
  }
  return glyphId;
}

/* kerning */
/* read kerning table */
uint8_t truetypeClass::readKern()
{
  uint32_t nextTable;

  if (this->seekToTable("kern") == 0)
  {
    // Serial0.println("kern not found");
    return 0;
  }

  kernHeader.nTables = this->getUInt32t();

  // only support up to 32 sub-tables
  if (kernHeader.nTables > 32)
  {
    kernHeader.nTables = 32;
  }

  for (uint8_t i = 0; i < kernHeader.nTables; i++)
  {
    uint16_t format;

    kernSubtable.length = this->getUInt32t();
    nextTable = file.position() + kernSubtable.length;
    kernSubtable.coverage = this->getUInt16t();

    format = (uint16_t)(kernSubtable.coverage >> 8);

    // only support format0
    if (format != 0)
    {
      file.seek(nextTable);
      continue;
    }

    // only use horizontal kerning tables
    if ((kernSubtable.coverage & 0x0003) != 0x0001)
    {
      file.seek(nextTable);
      continue;
    }

    // format0
    kernFormat0.nPairs = this->getUInt16t();
    kernFormat0.searchRange = this->getUInt16t();
    kernFormat0.entrySelector = this->getUInt16t();
    kernFormat0.rangeShift = this->getUInt16t();
    this->kernTablePos = file.position();

    break;
  }

  return 1;
}

int16_t truetypeClass::getKerning(uint16_t _left_glyph, uint16_t _right_glyph)
{
  // int16_t result = this->characterSpace;
  int16_t result = 0;
  uint32_t key0 = ((uint32_t)(_left_glyph) << 16) | (_right_glyph);

  file.seek(this->kernTablePos);
  for (uint16_t i = 0; i < kernFormat0.nPairs; i++)
  {
    uint32_t key1 = this->getUInt32t();
    if (key0 == key1)
    {
      result = this->getInt16t();
      break;
    }
    file.seek(file.position() + 2);
  }

  return result;
}

// hmtx. metric information for the horizontal layout each of the glyphs
uint8_t truetypeClass::readHMetric()
{
  if (this->seekToTable("hmtx") == 0)
  {
    // Serial0.println("hmtx not found");
    return 0;
  }

  this->hmtxTablePos = file.position();
  return 1;
}

ttHMetric_t truetypeClass::getHMetric(uint16_t _code)
{
  ttHMetric_t result;
  result.advanceWidth = 0;

  file.seek(this->hmtxTablePos + (_code * 4));
  result.advanceWidth = getUInt16t();
  result.leftSideBearing = getInt16t();

  result.advanceWidth = (result.advanceWidth * this->characterSize) / (this->yMax - this->yMin);
  result.leftSideBearing = (result.leftSideBearing * this->characterSize) / (this->yMax - this->yMin);
  return result;
}

/* get glyph offset */
uint32_t truetypeClass::getGlyphOffset(uint16_t index)
{
  uint32_t offset = 0;

  for (int i = 0; i < numTables; i++)
  {
    if (strcmp(table[i].name, "loca") == 0)
    {
      if (headTable.indexToLocFormat == 1)
      {
        file.seek(table[i].offset + index * 4);
        offset = this->getUInt32t();
      }
      else
      {
        file.seek(table[i].offset + index * 2);
        offset = this->getUInt16t() * 2;
      }
      break;
    }
  }

  for (int i = 0; i < numTables; i++)
  {
    if (strcmp(table[i].name, "glyf") == 0)
    {
      return (offset + table[i].offset);
    }
  }

  return 0;
}

/* read coords */
void truetypeClass::readCoords(char _xy, uint16_t _startPoint)
{
  int16_t value = 0;
  uint8_t shortFlag, sameFlag;

  if (_xy == 'x')
  {
    shortFlag = FLAG_XSHORT;
    sameFlag = FLAG_XSAME;
  }
  else
  {
    shortFlag = FLAG_YSHORT;
    sameFlag = FLAG_YSAME;
  }

  for (uint16_t i = _startPoint; i < glyph.numberOfPoints; i++)
  {
    if (glyph.points[i].flag & shortFlag)
    {
      if (glyph.points[i].flag & sameFlag)
      {
        value += this->getUInt8t();
      }
      else
      {
        value -= this->getUInt8t();
      }
    }
    else if (~glyph.points[i].flag & sameFlag)
    {
      value += this->getUInt16t();
    }

    if (_xy == 'x')
    {
      if (this->glyphTransformation.enableScale)
      {
        glyph.points[i].x = value + this->glyphTransformation.dx;
      }
      else
      {
        glyph.points[i].x = value + this->glyphTransformation.dx;
      }
    }
    else
    {
      if (this->glyphTransformation.enableScale)
      {
        glyph.points[i].y = value + this->glyphTransformation.dy;
      }
      else
      {
        glyph.points[i].y = value + this->glyphTransformation.dy;
      }
    }
  }
}

/* read simple glyph */
uint8_t truetypeClass::readSimpleGlyph(uint8_t _addGlyph)
{
  uint8_t repeatCount;
  uint8_t flag;
  static uint16_t counterContours;
  static uint16_t counterPoints;

  if (glyph.numberOfContours <= 0)
  {
    return 0;
  }

  if (!_addGlyph)
  {
    counterContours = 0;
    counterPoints = 0;
  }

  if (_addGlyph)
  {
    glyph.endPtsOfContours = (uint16_t *)realloc(glyph.endPtsOfContours, (sizeof(uint16_t) * glyph.numberOfContours));
  }
  else
  {
    glyph.endPtsOfContours = (uint16_t *)malloc((sizeof(uint16_t) * glyph.numberOfContours));
  }

  for (uint16_t i = counterContours; i < glyph.numberOfContours; i++)
  {
    glyph.endPtsOfContours[i] = counterPoints + this->getUInt16t();
  }

  file.seek(this->getUInt16t() + file.position());

  for (uint16_t i = counterContours; i < glyph.numberOfContours; i++)
  {
    if (glyph.endPtsOfContours[i] > glyph.numberOfPoints)
    {
      glyph.numberOfPoints = glyph.endPtsOfContours[i];
    }
  }
  glyph.numberOfPoints++;

  if (_addGlyph)
  {
    glyph.points = (ttPoint_t *)realloc(glyph.points, sizeof(ttPoint_t) * (glyph.numberOfPoints + glyph.numberOfContours));
  }
  else
  {
    glyph.points = (ttPoint_t *)malloc(sizeof(ttPoint_t) * (glyph.numberOfPoints + glyph.numberOfContours));
  }

  for (uint16_t i = counterPoints; i < glyph.numberOfPoints; i++)
  {
    flag = this->getUInt8t();
    glyph.points[i].flag = flag;
    if (flag & FLAG_REPEAT)
    {
      repeatCount = this->getUInt8t();
      while (repeatCount--)
      {
        glyph.points[++i].flag = flag;
      }
    }
  }

  this->readCoords('x', counterPoints);
  this->readCoords('y', counterPoints);

  counterContours = glyph.numberOfContours;
  counterPoints = glyph.numberOfPoints;

  return 1;
}

/* read Compound glyph */
uint8_t truetypeClass::readCompoundGlyph()
{
  uint16_t glyphIndex;
  uint16_t flags;
  uint8_t numberOfGlyphs = 0;
  uint32_t offset;
  int32_t arg1, arg2;

  glyph.numberOfContours = 0;

  // Serial0.println("---CompoundGlyph---");

  do
  {
    flags = this->getUInt16t();
    glyphIndex = this->getUInt16t();

    this->glyphTransformation.enableScale = (flags & 0b00000001000) ? (1) : (0);

    if (flags & 0b00000000001)
    {
      arg1 = this->getInt16t();
      arg2 = this->getInt16t();
    }
    else
    {
      arg1 = this->getUInt8t();
      arg2 = this->getUInt8t();
    }

    if (flags & 0b00000000010)
    {
      this->glyphTransformation.dx = arg1;
      this->glyphTransformation.dy = arg2;
    }

    if (flags & 0b01000000000)
    {
      this->charCode = glyphIndex;
    }

    // Serial0.printf("--%d: flag: 0x%04X index: %4d\n", numberOfGlyphs, flags, glyphIndex);
    // Serial0.printf("dx: %3d, dy: %3d\n", this->glyphTransformation.dx, this->glyphTransformation.dy);
    // Serial0.printf("Scaling: %d\n", this->glyphTransformation.enableScale);

    offset = file.position();

    uint32_t glyphOffset = this->getGlyphOffset(glyphIndex);
    file.seek(glyphOffset);
    glyph.numberOfContours += this->getInt16t();
    file.seek(glyphOffset + 10);

    if (numberOfGlyphs == 0)
    {
      this->readSimpleGlyph();
    }
    else
    {
      this->readSimpleGlyph(1);
    }
    file.seek(offset);

    numberOfGlyphs++;
    this->glyphTransformation = {0, 0, 0, 1, 1}; // init
  } while (flags & 0b00000100000);

  return 1;
}

/* read glyph */
uint8_t truetypeClass::readGlyph(uint16_t _code, uint8_t _justSize)
{
  uint32_t offset = this->getGlyphOffset(_code);
  file.seek(offset);
  glyph.numberOfContours = this->getInt16t();
  glyph.xMin = this->getInt16t();
  glyph.yMin = this->getInt16t();
  glyph.xMax = this->getInt16t();
  glyph.yMax = this->getInt16t();

  this->glyphTransformation = {0, 0, 0, 1, 1}; // init

  if (_justSize)
  {
    return 0;
  }

  if (glyph.numberOfContours >= 0)
  {
    return this->readSimpleGlyph();
  }
  else
  {
    return this->readCompoundGlyph();
  }
  return 0;
}

/* free glyph */

void truetypeClass::freeGlyph()
{
  if (glyph.points != NULL)
  {
    free(glyph.points);
    glyph.points = NULL;
  }
  if (glyph.endPtsOfContours != NULL)
  {
    free(glyph.endPtsOfContours);
    glyph.endPtsOfContours = NULL;
  }
  glyph.numberOfPoints = 0;
}

// generate Bitmap
void truetypeClass::generateOutline(int16_t _x, int16_t _y, uint16_t _width)
{
  this->points = NULL;
  this->numPoints = 0;
  this->numBeginPoints = 0;
  this->numEndPoints = 0;
  int16_t x0, y0, x1, y1;

  uint16_t j = 0;

  for (uint16_t i = 0; i < glyph.numberOfContours; i++)
  {
    uint8_t firstPointOfContour = j;
    uint8_t lastPointOfContour = glyph.endPtsOfContours[i];
    // Serial0.print("---Contour--- ");
    // Serial0.print(j);
    // Serial0.print(" , ");
    // Serial0.println(lastPointOfContour);

    // Rotate to on-curve the first point
    uint16_t numberOfRotations = 0;
    while ((firstPointOfContour + numberOfRotations) <= lastPointOfContour)
    {
      if (glyph.points[(firstPointOfContour + numberOfRotations)].flag & FLAG_ONCURVE)
      {
        break;
      }
      numberOfRotations++;
    }
    if ((j + numberOfRotations) <= lastPointOfContour)
    {
      for (uint16_t ii = 0; ii < numberOfRotations; ii++)
      {
        ttPoint_t tmp = glyph.points[firstPointOfContour];
        for (uint16_t jj = firstPointOfContour; jj < lastPointOfContour; jj++)
        {
          glyph.points[jj] = glyph.points[jj + 1];
        }
        glyph.points[lastPointOfContour] = tmp;
      }
    }

    while (j <= lastPointOfContour)
    {
      ttCoordinate_t pointsOfCurve[4];

      // Serial0.printf("%3d 0x%02X %5d %5d  - deg - ", j, glyph.points[j].flag, glyph.points[j].x, glyph.points[j].y);

      // Examine the number of dimensions of a curve
      pointsOfCurve[0].x = glyph.points[j].x;
      pointsOfCurve[0].y = glyph.points[j].y;
      uint16_t searchPoint = (j == lastPointOfContour) ? (firstPointOfContour) : (j + 1);
      uint8_t degree = 1;
      while (searchPoint != j)
      {
        // Serial0.printf("%5d 0x%02X %5d %5d  - ", searchPoint, glyph.points[searchPoint].flag, glyph.points[searchPoint].x, glyph.points[searchPoint].y);
        if (degree < 4)
        {
          pointsOfCurve[degree].x = glyph.points[searchPoint].x;
          pointsOfCurve[degree].y = glyph.points[searchPoint].y;
        }
        if (glyph.points[searchPoint].flag & FLAG_ONCURVE)
        {
          break;
        }
        searchPoint = (searchPoint == lastPointOfContour) ? (firstPointOfContour) : (searchPoint + 1);
        degree++;
      }

      // Serial0.printf(" ---- degree: %5d ", degree);
      // Replace Bezier curves of 4 dimensions or more with straight lines
      if (degree >= 4)
      {
        uint16_t tmp_j = j;
        uint16_t tmp_degree = 0;
        while (tmp_degree < degree)
        {
          if (tmp_j > lastPointOfContour)
          {
            tmp_j = firstPointOfContour;
          }
          glyph.points[tmp_j].flag |= FLAG_ONCURVE;
          tmp_j++;
          tmp_degree++;
        }
      }
      // Generate outline according to degree
      switch (degree)
      {
      case 3: // third-order Bezier curve
        x0 = pointsOfCurve[0].x;
        y0 = pointsOfCurve[0].y;

        for (float t = 0; t <= 1; t += 0.2)
        {
          x1 = (1 - t) * (1 - t) * (1 - t) * pointsOfCurve[0].x + 3 * (1 - t) * (1 - t) * t * pointsOfCurve[1].x + 3 * (1 - t) * t * t * pointsOfCurve[2].x + t * t * t * pointsOfCurve[3].x;
          y1 = (1 - t) * (1 - t) * (1 - t) * pointsOfCurve[0].y + 3 * (1 - t) * (1 - t) * t * pointsOfCurve[1].y + 3 * (1 - t) * t * t * pointsOfCurve[2].y + t * t * t * pointsOfCurve[3].y;

          this->addLine(map(x0, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                        map(y0, this->yMin, this->yMax, _y + this->characterSize - 1, _y),
                        map(x1, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                        map(y1, this->yMin, this->yMax, _y + this->characterSize - 1, _y));
          x0 = x1;
          y0 = y1;
        }
        break;
      case 2: // Second-order Bezier curve
        x0 = pointsOfCurve[0].x;
        y0 = pointsOfCurve[0].y;

        for (float t = 0; t <= 1; t += 0.2)
        {
          x1 = (1 - t) * (1 - t) * pointsOfCurve[0].x + 2 * t * (1 - t) * pointsOfCurve[1].x + t * t * pointsOfCurve[2].x;
          y1 = (1 - t) * (1 - t) * pointsOfCurve[0].y + 2 * t * (1 - t) * pointsOfCurve[1].y + t * t * pointsOfCurve[2].y;

          this->addLine(map(x0, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                        map(y0, this->yMin, this->yMax, _y + this->characterSize - 1, _y),
                        map(x1, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                        map(y1, this->yMin, this->yMax, _y + this->characterSize - 1, _y));
          x0 = x1;
          y0 = y1;
        }

        break;
      default:
        degree = 1;
      case 1: // straight line
        this->addLine(map(pointsOfCurve[0].x, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                      map(pointsOfCurve[0].y, this->yMin, this->yMax, _y + this->characterSize - 1, _y),
                      map(pointsOfCurve[1].x, glyph.xMin, glyph.xMax, _x, _x + _width - 1),
                      map(pointsOfCurve[1].y, this->yMin, this->yMax, _y + this->characterSize - 1, _y));
        break;
      }
      j += degree;
    }
    // Serial0.println(this->numPoints);
    this->addEndPoint(this->numPoints - 1);
    this->addBeginPoint(this->numPoints);
    // Serial0.println("---Contour end---");
  }
  return;
}

/* Bresenham's line algorithm */
void truetypeClass::addLine(int16_t _x0, int16_t _y0, int16_t _x1, int16_t _y1)
{
  // Serial0.printf("addLine(%3d, %3d) -> (%3d, %3d)\n", _x0, _y0, _x1, _y1);
  uint16_t dx = abs(_x1 - _x0);
  uint16_t dy = abs(_y1 - _y0);
  int16_t sx, sy, err, e2;

  if (this->numPoints == 0)
  {
    this->addPoint(_x0, _y0);
    this->addBeginPoint(0);
  }
  this->addPoint(_x1, _y1);

  if (_x0 < _x1)
  {
    sx = 1;
  }
  else
  {
    sx = -1;
  }
  if (_y0 < _y1)
  {
    sy = 1;
  }
  else
  {
    sy = -1;
  }
  err = dx - dy;

  while (1)
  {
    this->addPixel(_x0, _y0, this->colorLine);
    if ((_x0 == _x1) && (_y0 == _y1))
    {
      break;
    }
    e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      _x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      _y0 += sy;
    }
  }
}

bool truetypeClass::isInside(int16_t _x, int16_t _y)
{
  int16_t windingNumber = 0;
  uint16_t bpCounter = 0, epCounter = 0;
  ttCoordinate_t point = {_x, _y};
  ttCoordinate_t point1;
  ttCoordinate_t point2;

  for (uint16_t i = 0; i < this->numPoints; i++)
  {
    point1 = this->points[i];
    // Wrap?
    if (i == this->endPoints[epCounter])
    {
      point2 = this->points[this->beginPoints[bpCounter]];
      epCounter++;
      bpCounter++;
    }
    else
    {
      point2 = this->points[i + 1];
    }

    if (point1.y <= point.y)
    {
      if (point2.y > point.y)
      {
        if (this->isLeft(&point1, &point2, &point) > 0)
        {
          windingNumber++;
        }
      }
    }
    else
    {
      // start y > point.y (no test needed)
      if (point2.y <= point.y)
      {
        if (this->isLeft(&point1, &point2, &point) < 0)
        {
          windingNumber--;
        }
      }
    }
  }

  return (windingNumber != 0);
}

void truetypeClass::fillGlyph(uint16_t _x_min, uint16_t _y_min, uint16_t _width)
{
  for (uint16_t y = _y_min; y < (_y_min + this->characterSize); y++)
  {
    ttCoordinate_t point1, point2;
    ttCoordinate_t point;
    point.y = y;

    uint16_t intersectPointsNum = 0;
    uint16_t bpCounter = 0;
    uint16_t epCounter = 0;
    uint16_t p2Num = 0;

    for (uint16_t i = 0; i < numPoints; i++)
    {
      point1 = this->points[i];
      // Wrap?
      if (i == endPoints[epCounter])
      {
        p2Num = beginPoints[bpCounter];
        epCounter++;
        bpCounter++;
      }
      else
      {
        p2Num = i + 1;
      }
      point2 = this->points[p2Num];

      if (point1.y <= y)
      {
        if (point2.y > y)
        {
          // Have a valid up intersect
          intersectPointsNum++;
          pointsToFill = (ttWindIntersect_t *)realloc(pointsToFill, sizeof(ttWindIntersect_t) * intersectPointsNum);
          pointsToFill[intersectPointsNum - 1].p1 = i;
          pointsToFill[intersectPointsNum - 1].p2 = p2Num;
          pointsToFill[intersectPointsNum - 1].up = 1;
        }
      }
      else
      {
        // start y > point.y (no test needed)
        if (point2.y <= y)
        {
          // Have a valid down intersect
          intersectPointsNum++;
          pointsToFill = (ttWindIntersect_t *)realloc(pointsToFill, sizeof(ttWindIntersect_t) * intersectPointsNum);
          pointsToFill[intersectPointsNum - 1].p1 = i;
          pointsToFill[intersectPointsNum - 1].p2 = p2Num;
          pointsToFill[intersectPointsNum - 1].up = 0;
        }
      }
    }

    for (uint16_t x = _x_min; x < (_x_min + _width); x++)
    {
      int16_t windingNumber = 0;
      point.x = x;

      for (uint16_t i = 0; i < intersectPointsNum; i++)
      {
        point1 = this->points[pointsToFill[i].p1];
        point2 = this->points[pointsToFill[i].p2];

        if (pointsToFill[i].up == 1)
        {
          if (isLeft(&point1, &point2, &point) > 0)
          {
            windingNumber++;
          }
        }
        else
        {
          if (isLeft(&point1, &point2, &point) < 0)
          {
            windingNumber--;
          }
        }
      }

      if (windingNumber != 0)
      {
        this->addPixel(x, y, this->colorInside);
      }
    }
    if (pointsToFill != NULL)
    {
      free(pointsToFill);
      pointsToFill = NULL;
    }
  }
}

int32_t truetypeClass::isLeft(ttCoordinate_t *_p0, ttCoordinate_t *_p1, ttCoordinate_t *_point)
{
  return ((_p1->x - _p0->x) * (_point->y - _p0->y) - (_point->x - _p0->x) * (_p1->y - _p0->y));
}

void truetypeClass::_textDraw(int16_t _x, int16_t _y, const wchar_t _character[])
{
  uint16_t c = 0;
  uint16_t prev_code = 0;
  unsigned long begin = millis();
  while (_character[c] != '\0')
  {
    // space (half-width, full-width)
    if ((_character[c] == ' ') || (_character[c] == L'　'))
    {
      prev_code = 0;
      _x += this->characterSize / 4;
      c++;
      continue;
    }
    // Serial0.printf("%c\n", _character[c]);

    this->charCode = this->codeToGlyphId(_character[c]);
    // Serial0.printf("code:%4d\n", this->charCode);
    this->readGlyph(this->charCode);
    // Serial0.println(glyph.numberOfContours);

    _x += this->characterSpace;
    if (prev_code != 0 && this->kerningOn)
    {
      int16_t kern = this->getKerning(prev_code, this->charCode); // space between charctor
      _x += (kern * (int16_t)this->characterSize) / (this->yMax - this->yMin);
    }
    prev_code = this->charCode;

    ttHMetric_t hMetric = getHMetric(this->charCode);
    uint16_t width = this->characterSize * (glyph.xMax - glyph.xMin) / (this->yMax - this->yMin);

    // Line breaks when reaching the edge of the display
    if ((hMetric.leftSideBearing + width + _x) > this->end_x)
    {
      _x = this->start_x;
      _y += this->characterSize;
      if (_y > this->end_y)
      {
        break;
      }
    }

    // Line breaks with line feed code
    if (_character[c] == '\n')
    {
      _x = this->start_x;
      _y += this->characterSize;
      if (_y > this->end_y)
      {
        break;
      }
      continue;
    }

    // Not compatible with Compound glyphs now
    if (glyph.numberOfContours >= 0)
    {
      // write framebuffer
      this->generateOutline(hMetric.leftSideBearing + _x, _y, width);

      // fill charctor
      this->fillGlyph(hMetric.leftSideBearing + _x, _y, width);
    }
    this->freePointsAll();
    this->freeGlyph();

    // Serial0.println("---done");
    _x += (hMetric.advanceWidth) ? (hMetric.advanceWidth) : (width);
    c++;
  }

  log_i("绘制%ld个字符耗时%ldms", c, millis() - begin);
}

void truetypeClass::textDraw(int16_t _x, int16_t _y, const char _character[])
{
  uint16_t length = 0;
  while (_character[length] != '\0')
  {
    length++;
  }
  wchar_t *wcharacter = (wchar_t *)calloc(sizeof(wchar_t), length + 1);
  for (uint16_t i = 0; i < length; i++)
  {
    wcharacter[i] = _character[i];
  }
  this->_textDraw(_x, _y, wcharacter);
  free(wcharacter);
}

void truetypeClass::textDraw(int16_t _x, int16_t _y, const String _string)
{
  uint16_t length = _string.length();
  wchar_t *wcharacter = (wchar_t *)calloc(sizeof(wchar_t), length + 1);
  this->stringToWchar(_string, wcharacter);
  this->_textDraw(_x, _y, wcharacter);
  free(wcharacter);
}

void truetypeClass::addPixel(int16_t _x, int16_t _y, uint16_t _colorCode)
{
  // 限制坐标范围
  if ((_x < this->start_x) || (_x >= this->end_x) || (_y >= this->end_y))
  {
    return;
  }

  // 根据旋转角度转换坐标
  int16_t temp = _x;
  switch (this->stringRotation)
  {
  case ROTATE_270:
    _x = _y;
    _y = this->displayHeight - 1 - temp;
    break;
  case ROTATE_180:
    _x = this->displayWidth - 1 - _x;
    _y = this->displayHeight - 1 - _y;
    break;
  case ROTATE_90:
    _x = this->displayWidth - 1 - _y;
    _y = temp;
    break;
  case 0:
  default:
    break;
  }

  // 检查边界
  if ((_x < 0) || ((uint16_t)_x >= this->displayWidth) || ((uint16_t)_y >= this->displayHeight) || (_y < 0))
  {
    return;
  }

  if (this->drawBit == 1)
  {
    display.drawPixel(_x, _y, _colorCode);
    // log_i("ttf绘制 x:%d y:%d color:%04x", _x, _y, _colorCode);
  }
  else if (this->drawBit == 4)
  {
  }
}

uint16_t truetypeClass::getStringWidth(const wchar_t _character[])
{
  uint16_t prev_code = 0;
  uint16_t c = 0;
  uint16_t output = 0;

  while (_character[c] != '\0')
  {
    // space (half-width, full-width)
    if ((_character[c] == ' ') || (_character[c] == L'　'))
    {
      prev_code = 0;
      output += this->characterSize / 4;
      c++;
      continue;
    }
    uint16_t code = this->codeToGlyphId(_character[c]);
    this->readGlyph(code, 1);

    output += this->characterSpace;
    if (prev_code != 0 && this->kerningOn)
    {
      int16_t kern = this->getKerning(prev_code, code); // space between charctor
      output += (kern * (int16_t)this->characterSize) / (this->yMax - this->yMin);
    }
    prev_code = code;

    ttHMetric_t hMetric = getHMetric(code);
    uint16_t width = this->characterSize * (glyph.xMax - glyph.xMin) / (this->yMax - this->yMin);
    output += (hMetric.advanceWidth) ? (hMetric.advanceWidth) : (width);
    c++;
  }

  return output;
}

uint16_t truetypeClass::getStringWidth(const char _character[])
{
  uint16_t length = 0;
  uint16_t output = 0;
  while (_character[length] != '\0')
  {
    length++;
  }
  wchar_t *wcharacter = (wchar_t *)calloc(sizeof(wchar_t), length + 1);
  for (uint16_t i = 0; i < length; i++)
  {
    wcharacter[i] = _character[i];
  }
  output = this->getStringWidth(wcharacter);
  return output;
}

uint16_t truetypeClass::getStringWidth(const String _string)
{
  uint16_t length = _string.length();
  uint16_t output = 0;

  wchar_t *wcharacter = (wchar_t *)calloc(sizeof(wchar_t), length + 1);
  this->stringToWchar(_string, wcharacter);

  output = this->getStringWidth(wcharacter);
  return output;
}

/* Points*/
void truetypeClass::addPoint(int16_t _x, int16_t _y)
{
  this->numPoints++;
  this->points = (ttCoordinate_t *)realloc(this->points, sizeof(ttCoordinate_t) * this->numPoints);
  this->points[(this->numPoints - 1)].x = _x;
  this->points[(this->numPoints - 1)].y = _y;
}

void truetypeClass::addBeginPoint(uint16_t _bp)
{
  this->numBeginPoints++;
  this->beginPoints = (uint16_t *)realloc(this->beginPoints, sizeof(uint16_t) * this->numBeginPoints);
  this->beginPoints[(this->numBeginPoints - 1)] = _bp;
}

void truetypeClass::addEndPoint(uint16_t _ep)
{
  this->numEndPoints++;
  this->endPoints = (uint16_t *)realloc(this->endPoints, sizeof(uint16_t) * this->numEndPoints);
  this->endPoints[(this->numEndPoints - 1)] = _ep;
}

void truetypeClass::freePointsAll()
{
  this->freePoints();
  this->freeBeginPoints();
  this->freeEndPoints();
}

void truetypeClass::freePoints()
{
  if (this->points != NULL)
  {
    free(this->points);
    this->points = NULL;
    this->numPoints = 0;
  }
}

void truetypeClass::freeBeginPoints()
{
  if (this->beginPoints != NULL)
  {
    free(this->beginPoints);
    this->beginPoints = NULL;
    this->numBeginPoints = 0;
  }
}

void truetypeClass::freeEndPoints()
{
  if (this->endPoints != NULL)
  {
    free(this->endPoints);
    this->endPoints = NULL;
    this->numEndPoints = 0;
  }
}

/* file */
/* seek to the first position of the specified table name */
uint32_t truetypeClass::seekToTable(const char *name)
{
  for (uint32_t i = 0; i < this->numTables; i++)
  {
    if (strcmp(table[i].name, name) == 0)
    {
      file.seek(table[i].offset);
      return table[i].offset;
    }
  }
  return 0;
}

/* calculate */
void truetypeClass::stringToWchar(String _string, wchar_t _charctor[])
{
  uint16_t s = 0; // 字符串索引
  uint16_t c = 0; // 宽字符数组索引

  while (s < _string.length())
  {
    uint8_t firstByte = _string[s];

    if (firstByte < 0x80)
    {
      // 单字节 ASCII 字符
      _charctor[c++] = firstByte;
      s++;
    }
    else if ((firstByte & 0xE0) == 0xC0)
    {
      // 2字节 UTF-8 字符
      if (s + 1 < _string.length())
      {
        uint8_t secondByte = _string[s + 1];
        if ((secondByte & 0xC0) == 0x80)
        {
          _charctor[c++] = ((firstByte & 0x1F) << 6) | (secondByte & 0x3F);
          s += 2;
        }
        else
        {
          s++; // 无效序列，跳过
        }
      }
      else
      {
        s++; // 不完整的序列，跳过
      }
    }
    else if ((firstByte & 0xF0) == 0xE0)
    {
      // 3字节 UTF-8 字符（包含中文）
      if (s + 2 < _string.length())
      {
        uint8_t secondByte = _string[s + 1];
        uint8_t thirdByte = _string[s + 2];
        if ((secondByte & 0xC0) == 0x80 && (thirdByte & 0xC0) == 0x80)
        {
          _charctor[c++] = ((firstByte & 0x0F) << 12) |
                           ((secondByte & 0x3F) << 6) |
                           (thirdByte & 0x3F);
          s += 3;
        }
        else
        {
          s++; // 无效序列，跳过
        }
      }
      else
      {
        s++; // 不完整的序列，跳过
      }
    }
    else if ((firstByte & 0xF8) == 0xF0)
    {
      // 4字节 UTF-8 字符，需要代理对
      if (s + 3 < _string.length())
      {
        uint8_t secondByte = _string[s + 1];
        uint8_t thirdByte = _string[s + 2];
        uint8_t fourthByte = _string[s + 3];
        if ((secondByte & 0xC0) == 0x80 && (thirdByte & 0xC0) == 0x80 &&
            (fourthByte & 0xC0) == 0x80)
        {
          // 计算 Unicode 码点
          uint32_t codePoint = ((firstByte & 0x07) << 18) |
                               ((secondByte & 0x3F) << 12) |
                               ((thirdByte & 0x3F) << 6) |
                               (fourthByte & 0x3F);

          if (codePoint > 0xFFFF)
          {
            // 需要 UTF-16 代理对
            codePoint -= 0x10000;
            _charctor[c++] = 0xD800 | (codePoint >> 10);
            _charctor[c++] = 0xDC00 | (codePoint & 0x3FF);
          }
          else
          {
            _charctor[c++] = codePoint;
          }
          s += 4;
        }
        else
        {
          s++; // 无效序列，跳过
        }
      }
      else
      {
        s++; // 不完整的序列，跳过
      }
    }
    else
    {
      s++; // 无效的 UTF-8 起始字节，跳过
    }
  }
  _charctor[c] = 0; // 终止符
  log_i("Converted %d bytes to %d wchars", s, c);
  for (int i = 0; i < c; i++)
  {
    log_i("wchar[%d] = 0x%04X", i, _charctor[i]);
  }
}

uint8_t truetypeClass::GetU8ByteCount(char _ch)
{
  if (0 <= uint8_t(_ch) && uint8_t(_ch) < 0x80)
  {
    return 1;
  }
  if (0xC2 <= uint8_t(_ch) && uint8_t(_ch) < 0xE0)
  {
    return 2;
  }
  if (0xE0 <= uint8_t(_ch) && uint8_t(_ch) < 0xF0)
  {
    return 3;
  }
  if (0xF0 <= uint8_t(_ch) && uint8_t(_ch) < 0xF8)
  {
    return 4;
  }
  return 0;
}

bool truetypeClass::IsU8LaterByte(char _ch)
{
  return 0x80 <= uint8_t(_ch) && uint8_t(_ch) < 0xC0;
}

/* get uint8_t at the current position */
uint8_t truetypeClass::getUInt8t()
{
  uint8_t x;

  file.read(&x, 1);
  return x;
}

/* get int16_t at the current position */
int16_t truetypeClass::getInt16t()
{
  byte x[2];

  file.read(x, 2);
  return (x[0] << 8) | x[1];
}

/* get uint16_t at the current position */
uint16_t truetypeClass::getUInt16t()
{
  byte x[2];

  file.read(x, 2);
  return (x[0] << 8) | x[1];
}

/* get uint32_t at the current position */
uint32_t truetypeClass::getUInt32t()
{
  byte x[4];

  file.read(x, 4);
  return (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
}

// /* ---------------- 16阶灰度抗锯齿实现 ---------------- */

// // 初始化灰度缓冲区
// void truetypeClass::initGrayBuffer() {
//     if (this->grayBuffer != NULL) {
//         free(this->grayBuffer);
//     }

//     this->grayBufferSize = (this->displayWidth * this->displayHeight + 1) / 2; // 4bit packed
//     this->grayBuffer = (uint8_t*)malloc(this->grayBufferSize);
//     if (this->grayBuffer != NULL) {
//         memset(this->grayBuffer, 0x00, this->grayBufferSize); // 初始化为白色(0)
//     }
// }

// // 初始化超采样缓冲区
// void truetypeClass::initSuperSampleBuffer(uint16_t width, uint16_t height) {
//     if (this->superSampleBuffer != NULL) {
//         free(this->superSampleBuffer);
//     }

//     this->superSampleWidth = width * SUPER_SAMPLING;
//     this->superSampleHeight = height * SUPER_SAMPLING;
//     uint32_t bufferSize = this->superSampleWidth * this->superSampleHeight;
//     this->superSampleBuffer = (uint8_t*)malloc(bufferSize);
//     if (this->superSampleBuffer != NULL) {
//         memset(this->superSampleBuffer, 0, bufferSize); // 初始化为0
//     }
// }

// // 释放超采样缓冲区
// void truetypeClass::freeSuperSampleBuffer() {
//     if (this->superSampleBuffer != NULL) {
//         free(this->superSampleBuffer);
//         this->superSampleBuffer = NULL;
//     }
// }

// // 写入4bit灰度像素到缓冲区
// void truetypeClass::writeGrayPixel(uint16_t x, uint16_t y, uint8_t gray) {
//     if (x >= this->displayWidth || y >= this->displayHeight || gray >= GRAY_LEVELS) {
//         return;
//     }

//     uint32_t index = y * this->displayWidth + x;
//     uint32_t byteIndex = index / 2;
//     uint8_t shift = (index % 2) ? 0 : 4;

//     if (byteIndex < this->grayBufferSize) {
//         this->grayBuffer[byteIndex] = (this->grayBuffer[byteIndex] & (0x0F >> shift)) | (gray << shift);
//     }
// }

// // 读取4bit灰度像素
// uint8_t truetypeClass::readGrayPixel(uint16_t x, uint16_t y) {
//     if (x >= this->displayWidth || y >= this->displayHeight) {
//         return 0;
//     }

//     uint32_t index = y * this->displayWidth + x;
//     uint32_t byteIndex = index / 2;
//     uint8_t shift = (index % 2) ? 0 : 4;

//     if (byteIndex < this->grayBufferSize) {
//         return (this->grayBuffer[byteIndex] >> shift) & 0x0F;
//     }
//     return 0;
// }

// // 超采样坐标转换
// void truetypeClass::superSampleAddPixel(int16_t x, int16_t y) {
//     if (this->superSampleBuffer == NULL ||
//         x < 0 || x >= this->superSampleWidth ||
//         y < 0 || y >= this->superSampleHeight) {
//         return;
//     }

//     this->superSampleBuffer[y * this->superSampleWidth + x] = 1;
// }

// // 超采样的Bresenham直线算法
// void truetypeClass::addLineGray(int16_t _x0, int16_t _y0, int16_t _x1, int16_t _y1) {
//     // 转换为超采样坐标
//     int16_t x0 = _x0 * SUPER_SAMPLING;
//     int16_t y0 = _y0 * SUPER_SAMPLING;
//     int16_t x1 = _x1 * SUPER_SAMPLING;
//     int16_t y1 = _y1 * SUPER_SAMPLING;

//     uint16_t dx = abs(x1 - x0);
//     uint16_t dy = abs(y1 - y0);
//     int16_t sx, sy, err, e2;

//     if (x0 < x1) {
//         sx = 1;
//     } else {
//         sx = -1;
//     }
//     if (y0 < y1) {
//         sy = 1;
//     } else {
//         sy = -1;
//     }
//     err = dx - dy;

//     while (1) {
//         this->superSampleAddPixel(x0, y0);
//         if ((x0 == x1) && (y0 == y1)) {
//             break;
//         }
//         e2 = 2 * err;
//         if (e2 > -dy) {
//             err -= dy;
//             x0 += sx;
//         }
//         if (e2 < dx) {
//             err += dx;
//             y0 += sy;
//         }
//     }
// }

// // 超采样版本的填充算法
// void truetypeClass::fillGlyphGray(uint16_t _x_min, uint16_t _y_min, uint16_t _width) {
//     if (this->superSampleBuffer == NULL) return;

//     uint16_t super_x_min = _x_min * SUPER_SAMPLING;
//     uint16_t super_y_min = _y_min * SUPER_SAMPLING;
//     uint16_t super_width = _width * SUPER_SAMPLING;
//     uint16_t super_height = this->characterSize * SUPER_SAMPLING;

//     // 使用扫描线填充算法
//     for (uint16_t y = super_y_min; y < super_y_min + super_height; y++) {
//         for (uint16_t x = super_x_min; x < super_x_min + super_width; x++) {
//             if (this->isInsideSuperSample(x, y)) {
//                 this->superSampleAddPixel(x, y);
//             }
//         }
//     }
// }

// // 超采样版本的边界检测
// bool truetypeClass::isInsideSuperSample(int16_t _x, int16_t _y) {
//     int16_t windingNumber = 0;
//     uint16_t bpCounter = 0, epCounter = 0;
//     ttCoordinate_t point = {_x, _y};
//     ttCoordinate_t point1;
//     ttCoordinate_t point2;

//     for (uint16_t i = 0; i < this->numPoints; i++) {
//         point1 = this->points[i];
//         // Wrap?
//         if (i == this->endPoints[epCounter]) {
//             point2 = this->points[this->beginPoints[bpCounter]];
//             epCounter++;
//             bpCounter++;
//         } else {
//             point2 = this->points[i + 1];
//         }

//         // 转换为超采样坐标
//         point1.x *= SUPER_SAMPLING;
//         point1.y *= SUPER_SAMPLING;
//         point2.x *= SUPER_SAMPLING;
//         point2.y *= SUPER_SAMPLING;

//         if (point1.y <= point.y) {
//             if (point2.y > point.y) {
//                 if (this->isLeft(&point1, &point2, &point) > 0) {
//                     windingNumber++;
//                 }
//             }
//         } else {
//             if (point2.y <= point.y) {
//                 if (this->isLeft(&point1, &point2, &point) < 0) {
//                     windingNumber--;
//                 }
//             }
//         }
//     }

//     return (windingNumber != 0);
// }

// // 将超采样缓冲区下采样到灰度缓冲区
// void truetypeClass::downsampleSuperSampleToGray(uint16_t _x_min, uint16_t _y_min, uint16_t _width, uint16_t _height) {
//     if (this->superSampleBuffer == NULL) return;

//     for (uint16_t y = 0; y < _height; y++) {
//         for (uint16_t x = 0; x < _width; x++) {
//             uint32_t sampleCount = 0;
//             uint32_t hitCount = 0;

//             // 统计超采样区域
//             for (uint8_t sy = 0; sy < SUPER_SAMPLING; sy++) {
//                 for (uint8_t sx = 0; sx < SUPER_SAMPLING; sx++) {
//                     uint16_t super_x = x * SUPER_SAMPLING + sx;
//                     uint16_t super_y = y * SUPER_SAMPLING + sy;

//                     if (super_x < this->superSampleWidth && super_y < this->superSampleHeight) {
//                         sampleCount++;
//                         if (this->superSampleBuffer[super_y * this->superSampleWidth + super_x]) {
//                             hitCount++;
//                         }
//                     }
//                 }
//             }

//             if (sampleCount > 0) {
//                 // 计算灰度值 (0-15)，数值越大越黑
//                 uint8_t gray = (hitCount * (GRAY_LEVELS - 1) + sampleCount / 2) / sampleCount;

//                 // 应用轮廓和填充的颜色设置
//                 uint16_t target_x = _x_min + x;
//                 uint16_t target_y = _y_min + y;

//                 if (target_x < this->displayWidth && target_y < this->displayHeight) {
//                     // 检查当前颜色设置
//                     if (this->colorLine != GRAY_SCALE_WHITE) {
//                         // 正常绘制，使用计算的灰度值
//                         this->writeGrayPixel(target_x, target_y, gray);
//                     }
//                     // 如果colorLine为0xFFFF，则不绘制（保持原背景）
//                 }
//             }
//         }
//     }
// }

// // 刷新灰度缓冲区到屏幕
// void truetypeClass::buildGraytemp() {
//   if (this->drawBit != 4 || this->grayBuffer == NULL) {
//       return;
//   }
//   HEADGRAY header;
//   header.gray = 4;
//   header.w = 296;
//   header.h = 128;
//   header.scan = 0;
//   File temp = LittleFS.open("/System/ttf_temp.lbm", "w");
//   temp.write((uint8_t *)&header, sizeof(header));
//   temp.write(grayBuffer, grayBufferSize);
//   temp.close();
//   free(grayBuffer);
//   grayBuffer = NULL;
// }