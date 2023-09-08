#include "2d.h"

typedef u32 Fixed;
typedef i64 longDateTime;
typedef i16 FWord;

struct TableDirectory {
	u32	tag;
	u32	checkSum;
	u32	offset;
	u32	length;
};

struct OffsetSubTable {
	u32 scalerType;
	u16 numTables;
	u16 searchRange;
	u16 entrySelector;
	u16 rangeShift;
};

struct FontDirectory {
	struct OffsetSubTable offsetSubTable;
	struct TableDirectory tableDirectory[];
};

struct __attribute__((packed)) head {
	Fixed version;
	Fixed fontRevision;
	u32 checkSumAdjustment;
	u32 magicNumber;
	u16 flags;
	u16 unitsPerEm;
	longDateTime created;
	longDateTime modified;
	FWord xMin;
	FWord yMin;
	FWord xMax;
	FWord yMax;
	u16 macStyle;
	u16 lowestRecPPEM;
	i16 fontDirectionHint;
	i16 indexToLocFormat;
	i16 glyphDataFormat;
};

struct cmapEncodingSubtable {
	u16 platformID;
	u16 platformSpecificID;
	u32 offset;
};

struct cmap {
	u16 version;
	u16 numberSubtables;
	struct cmapEncodingSubtable subtables[];
};

struct format4 {
	u16 format;
	u16 length;
	u16 language;
	u16 segCountX2;
	u16 searchRange;
	u16 entrySelector;
	u16 rangeShift;
	u16 endCode[]; // [segCountX2 / 2];
	// u16 reservedPad;
	// u16 startCode[segCountX2 / 2];
	// u16 idDelta[segCountX2 / 2];
	// u16 idRangeOffset[segCountX2 / 2];
	// u16 glyphIdArray[segCountX2 / 2];
};

struct GlyphDescription {
	i16 numberOfContours;
	FWord xMin;
	FWord yMin;
	FWord xMax;
	FWord yMax;
};

struct GlyphFlags {
	u8 onCurve : 1;
	u8 xShort : 1;
	u8 yShort : 1;
	u8 repeat : 1;
	u8 xShortPos : 1;
	u8 yShortPos : 1;
};

// struct SimpleGlyph {
// 	u16 endPtsOfContours[];
//	u16 instructionLength;
//	u8 instructions[instructionLength];
//	u8 flags[variable];
//	u8|u16 xCoordinates[];
//	u8|u16 yCoordinates[];
// };

static inline void createFontBitmap(u8* imageData) {
	HANDLE consola;
	if (!(consola = CreateFileW(L"C:/Windows/Fonts/consola.ttf", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)))
		win32Fatal("CreateFileW", GetLastError());

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(consola, &fileSize))
		win32Fatal("GetFileSizeEx", GetLastError());

	static char buff[1 << 19];

	DWORD bytesRead;
	if (!ReadFile(consola, buff, (DWORD)fileSize.QuadPart, &bytesRead, NULL))
		win32Fatal("ReadFile", GetLastError());

	struct FontDirectory* fontDirectory = (struct FontDirectory*)buff;

	fontDirectory->offsetSubTable.scalerType = __builtin_bswap32(fontDirectory->offsetSubTable.scalerType);
	fontDirectory->offsetSubTable.numTables = __builtin_bswap16(fontDirectory->offsetSubTable.numTables);
	fontDirectory->offsetSubTable.searchRange = __builtin_bswap16(fontDirectory->offsetSubTable.searchRange);
	fontDirectory->offsetSubTable.entrySelector = __builtin_bswap16(fontDirectory->offsetSubTable.entrySelector);
	fontDirectory->offsetSubTable.rangeShift = __builtin_bswap16(fontDirectory->offsetSubTable.rangeShift);

	char* glyf;
	void* loca;
	struct head* head;
	struct cmap* cmap;

	for (u16 i = 0; i < fontDirectory->offsetSubTable.numTables; i++) {
		fontDirectory->tableDirectory[i].tag = __builtin_bswap32(fontDirectory->tableDirectory[i].tag);
		fontDirectory->tableDirectory[i].checkSum = __builtin_bswap32(fontDirectory->tableDirectory[i].checkSum);
		fontDirectory->tableDirectory[i].offset = __builtin_bswap32(fontDirectory->tableDirectory[i].offset);
		fontDirectory->tableDirectory[i].length = __builtin_bswap32(fontDirectory->tableDirectory[i].length);

		switch (fontDirectory->tableDirectory[i].tag) {
			case 'glyf':
				glyf = buff + fontDirectory->tableDirectory[i].offset;
				break;
			case 'loca':
				loca = buff + fontDirectory->tableDirectory[i].offset;
				break;
			case 'head':
				head = (struct head*)__builtin_assume_aligned(buff + fontDirectory->tableDirectory[i].offset, alignof(struct head));
				head->version = __builtin_bswap32(head->version);
				head->fontRevision = __builtin_bswap32(head->fontRevision);
				head->checkSumAdjustment = __builtin_bswap32(head->checkSumAdjustment);
				head->magicNumber = __builtin_bswap32(head->magicNumber);
				head->flags = __builtin_bswap16(head->flags);
				head->unitsPerEm = __builtin_bswap16(head->unitsPerEm);
				head->created = (i64)__builtin_bswap64((u64)head->created);
				head->modified = (i64)__builtin_bswap64((u64)head->modified);
				head->xMin = (i16)__builtin_bswap16((u16)head->xMin);
				head->yMin = (i16)__builtin_bswap16((u16)head->yMin);
				head->xMax = (i16)__builtin_bswap16((u16)head->xMax);
				head->yMax = (i16)__builtin_bswap16((u16)head->yMax);
				head->macStyle = __builtin_bswap16(head->macStyle);
				head->lowestRecPPEM = __builtin_bswap16(head->lowestRecPPEM);
				head->fontDirectionHint = (i16)__builtin_bswap16((u16)head->fontDirectionHint);
				head->indexToLocFormat = (i16)__builtin_bswap16((u16)head->indexToLocFormat);
				head->glyphDataFormat = (i16)__builtin_bswap16((u16)head->glyphDataFormat);
				break;
			case 'cmap':
				cmap = (struct cmap*)__builtin_assume_aligned(buff + fontDirectory->tableDirectory[i].offset, alignof(struct cmap));
				cmap->version = __builtin_bswap16(cmap->version);
				cmap->numberSubtables = __builtin_bswap16(cmap->numberSubtables);
				break;
		}
	}

	struct format4* f4;
	u16* reservedPad;
	u16* startCode;
	u16* idDelta;
	u16* idRangeOffset;
	u16* glyphIdArray;

	u16 glyphCount;

	for (u16 i = 0; i < cmap->numberSubtables; i++) {
		cmap->subtables[i].platformID = __builtin_bswap16(cmap->subtables[i].platformID);
		cmap->subtables[i].platformSpecificID = __builtin_bswap16(cmap->subtables[i].platformSpecificID);
		cmap->subtables[i].offset = __builtin_bswap32(cmap->subtables[i].offset);

		f4 = (void*)((char*)cmap + cmap->subtables[i].offset);
		f4->format = __builtin_bswap16(f4->format);

		if (cmap->subtables[i].platformID == 0 && cmap->subtables[i].platformSpecificID == 3 && f4->format == 4) {
			f4->length = __builtin_bswap16(f4->length);
			f4->language = __builtin_bswap16(f4->language);
			f4->segCountX2 = __builtin_bswap16(f4->segCountX2);
			f4->searchRange = __builtin_bswap16(f4->searchRange);
			f4->entrySelector = __builtin_bswap16(f4->entrySelector);
			f4->rangeShift = __builtin_bswap16(f4->rangeShift);

			reservedPad = f4->endCode + f4->segCountX2 / 2;
			startCode = reservedPad + 1;
			idDelta = startCode + f4->segCountX2 / 2;
			idRangeOffset = idDelta + f4->segCountX2 / 2;
			glyphIdArray = idRangeOffset + f4->segCountX2 / 2;

			for (u16 j = 0; j < f4->segCountX2 / 2; j++) {
				f4->endCode[j] = __builtin_bswap16(f4->endCode[j]);
				startCode[j] = __builtin_bswap16(startCode[j]);
				idDelta[j] = __builtin_bswap16(idDelta[j]);
				idRangeOffset[j] = __builtin_bswap16(idRangeOffset[j]);
			}

			glyphCount = (f4->length - (u16)((char*)glyphIdArray - (char*)f4)) / 2;

			for (u16 j = 0; j < glyphCount; j++)
				glyphIdArray[j] = __builtin_bswap16(glyphIdArray[j]);

			break;
		}
	}

	u16 codePoint = 'A';

	u16 glyphIndex;

	u16 index = (u16)-1;
	u16 *ptr = NULL;
	for (u16 i = 0; i < f4->segCountX2 / 2; i++)
		if (f4->endCode[i] > codePoint) {
			index = i;

			if (startCode[index] < codePoint) {
				if (idRangeOffset[index] != 0) {
					ptr = idRangeOffset + index + idRangeOffset[index] / 2;
					ptr += codePoint - startCode[index];

					if (*ptr == 0) {
						glyphIndex = 0;
						break;
					}

					glyphIndex = *ptr + idDelta[index];
				} else {
					glyphIndex = codePoint + idDelta[index];
				}
				goto ok;
			}

			break;
		}

	glyphIndex = 0;

ok:
	u32 offset;
	if (head->indexToLocFormat)
		offset = __builtin_bswap32(((u32*)loca)[glyphIndex]);
	else
		offset = __builtin_bswap16(((u16*)loca)[glyphIndex]) * 2;

	struct GlyphDescription* glyphDescription = (struct GlyphDescription*)__builtin_assume_aligned(glyf + offset, alignof(struct GlyphDescription));
	glyphDescription->numberOfContours = (i16)__builtin_bswap16((u16)glyphDescription->numberOfContours);
	glyphDescription->xMin = (i16)__builtin_bswap16((u16)glyphDescription->xMin);
	glyphDescription->yMin = (i16)__builtin_bswap16((u16)glyphDescription->yMin);
	glyphDescription->xMax = (i16)__builtin_bswap16((u16)glyphDescription->xMax);
	glyphDescription->yMax = (i16)__builtin_bswap16((u16)glyphDescription->yMax);

	u16* endPtsOfContours = (u16*)(glyphDescription + 1);
	for (u16 j = 0; j < glyphDescription->numberOfContours; j++)
		endPtsOfContours[j] = __builtin_bswap16(endPtsOfContours[j]);

	u16* instructionLength = endPtsOfContours + glyphDescription->numberOfContours;
	*instructionLength = __builtin_bswap16(*instructionLength);

	u8* instructions = (u8*)(instructionLength + 1);

	struct GlyphFlags* flags = (struct GlyphFlags*)(instructions + *instructionLength);

	u16 lastIndex = endPtsOfContours[glyphDescription->numberOfContours - 1];

	u16 j = 0;
	for (; j <= lastIndex; j++) {
		if (flags[j].repeat) {
			u8 repeatCount = ((u8*)flags)[j + 1];
			while (repeatCount-- > 0) {
				flags[j + 1] = flags[j];
				j++;
			}
		}
	}

	i16 prevCoordinate = 0;
	i16 currentCoordinate = 0;

	void* xCoordinates = flags + lastIndex;

	i16 result[512];

	for (j = 0; j <= lastIndex; j++) {
		u8 combined = (u8)(flags[j].xShort << 1 | flags[j].xShortPos);

		switch (combined) {
			case 0:
				currentCoordinate = (i16)__builtin_bswap16(*(u16*)xCoordinates);
				xCoordinates += sizeof(i16);
				break;
			case 1:
				currentCoordinate = 0;
				break;
			case 2:
				currentCoordinate = *(u8*)xCoordinates++ * -1;
				break;
			case 3:
				currentCoordinate = *(u8*)xCoordinates++;
				break;
		}

		result[j] = currentCoordinate + prevCoordinate;
		prevCoordinate = result[j];
	}

	// __debugbreak();

}

static inline u32 textWidth(const char* text, float size) {
	return 0;
}

static inline void drawText(const char* text, i32 x, i32 y, float size, u8vec4 color) {
	u32 vertexOffset = verticesTextCount;


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_TEXT]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_DEVICE].handle, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(commandBuffer, ((verticesTextCount - vertexOffset) / 4) * 6, 1, BUFFER_OFFSET_QUAD_INDICES / sizeof(u16), (int32_t)vertexOffset, 0);
}
