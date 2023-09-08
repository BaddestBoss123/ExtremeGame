#include "assets.h"

enum LineJoin {
	LINE_JOIN_MITER,
	LINE_JOIN_ROUND
};

enum LineCap {
	LINE_CAP_BUTT,
	LINE_CAP_ROUND,
	LINE_CAP_SQUARE
};

struct SubPath2D {
	u32 pointCount;
	bool closed;
	vec2* points;
};

struct Path2D {
	u32 subPathCount;
	struct SubPath2D* subPaths;
};

static mat4 viewport;

static u16* indices2D;
static u32 indices2DCount;

static vec2* vertices2D;
static u32 vertices2DCount;

static vec4* verticesText;
static u32 verticesTextCount;

static struct Path2D path = {
	.subPathCount = 0,
	.subPaths = (struct SubPath2D[]){
		{
			.points = (vec2[512]){ 0 }
		}, {
			.points = (vec2[512]){ 0 }
		}, {
			.points = (vec2[512]){ 0 }
		}
	}
};

static inline void setTransform(mat4 transform) {
	mat4 m = viewport * transform;
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &m);
}

static inline void beginPath(void) {
	path.subPaths[0].pointCount = 0;
	path.subPaths[0].closed = false;
	path.subPathCount = 1;
}

static inline void bezierCurveTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
	struct SubPath2D* p = &path.subPaths[path.subPathCount - 1];

	vec2 start = p->points[p->pointCount - 1];

	u32 steps = 32;
	for (u32 i = 1; i <= steps; i++) {
		float t = (float)i / (float)steps;
		float u = 1.0f - t;

		p->points[p->pointCount++] = (vec2){
			u * u * u * start.x + 3 * u * u * t * c1x + 3 * u * t * t * c2x + t * t * t * x,
			u * u * u * start.y + 3 * u * u * t * c1y + 3 * u * t * t * c2y + t * t * t * y
		};
	}
}

static inline void closePath(void) {
	struct SubPath2D* p = &path.subPaths[path.subPathCount - 1];
	p->closed = true;
}

static inline void moveTo(float x, float y) {
	struct SubPath2D* p;

	if (path.subPaths[path.subPathCount - 1].pointCount == 0)
		p = &path.subPaths[path.subPathCount - 1];
	else
		p = &path.subPaths[path.subPathCount++];

	p->pointCount = 1;
	p->closed = false;
	p->points[0] = (vec2){ x, y };
}

static inline void lineTo(float x, float y) {
	struct SubPath2D* p = &path.subPaths[path.subPathCount - 1];
	p->points[p->pointCount++] = (vec2){ x, y };
}

static inline void arc(float x, float y, float radius, float start, float end) {
	struct SubPath2D* p;

	if (path.subPaths[path.subPathCount - 1].pointCount == 0)
		p = &path.subPaths[path.subPathCount - 1];
	else {
		p = &path.subPaths[path.subPathCount++];
		p->pointCount = 0;
	}

	p->closed = true;

	// TODO: fix the calculation to be more efficient
	u32 steps = (u32)__builtin_fminf(256.f, ((end - start) * radius / M_PI));

	float stepAngle = (end - start) / (float)steps;

	for (u32 i = 0; i < steps; i++) {
		float angle = normalizeAngle(start + (float)i * stepAngle);
		float px = x + radius * __builtin_cosf(angle);
		float py = y + radius * __builtin_sinf(angle);
		p->points[p->pointCount++] = (vec2){ px, py };
	}
}

static inline void rect(float x, float y, float width, float height) {
	struct SubPath2D* p;

	if (path.subPaths[path.subPathCount - 1].pointCount == 0)
		p = &path.subPaths[path.subPathCount - 1];
	else
		p = &path.subPaths[path.subPathCount++];

	p->closed = true;

	p->points[0] = (vec2){ x, y };
	p->points[1] = (vec2){ x + width, y };
	p->points[2] = (vec2){ x + width, y + height };
	p->points[3] = (vec2){ x, y + height };
	p->pointCount = 4;
}

static inline void fill(u8vec4 color) {
	u32 firstIndex = indices2DCount;
	i32 vertexOffset = (i32)vertices2DCount;

	u16 firstVertexIndex = 0;
	for (u32 i = 0; i < path.subPathCount; i++) {
		struct SubPath2D* p = &path.subPaths[i];

		for (u16 j = 0; j < p->pointCount; j++) {
			vertices2D[vertices2DCount++] = p->points[j];

			if (j >= 2) {
				indices2D[indices2DCount++] = firstVertexIndex;
				indices2D[indices2DCount++] = firstVertexIndex + j - 1;
				indices2D[indices2DCount++] = firstVertexIndex + j;
			}
		}

		firstVertexIndex += p->pointCount;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_PATH2D]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_FRAME].handle, BUFFER_OFFSET_INDICES_2D + frame * BUFFER_RANGE_INDICES_2D, VK_INDEX_TYPE_UINT16);

	vec4 col = (vec4){ color.x, color.y, color.z, color.w } / 255.f;
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(vec4), &col);
	vkCmdDrawIndexed(commandBuffer, indices2DCount - firstIndex, 1, firstIndex, vertexOffset, 0);
}

static inline void clip(void) {
	u32 firstIndex = indices2DCount;
	i32 vertexOffset = (i32)vertices2DCount;

	u16 firstVertexIndex = 0;
	for (u32 i = 0; i < path.subPathCount; i++) {
		struct SubPath2D* p = &path.subPaths[i];

		for (u16 j = 0; j < p->pointCount; j++) {
			vertices2D[vertices2DCount++] = p->points[j];

			if (j >= 2) {
				indices2D[indices2DCount++] = firstVertexIndex;
				indices2D[indices2DCount++] = firstVertexIndex + j - 1;
				indices2D[indices2DCount++] = firstVertexIndex + j;
			}
		}

		firstVertexIndex += p->pointCount;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_PATH2D_CLIP]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_FRAME].handle, BUFFER_OFFSET_INDICES_2D + frame * BUFFER_RANGE_INDICES_2D, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(commandBuffer, indices2DCount - firstIndex, 1, firstIndex, vertexOffset, 0);
}

static inline void stroke(u8vec4 color, float lineWidth, enum LineJoin join, enum LineCap cap) {
	u32 firstIndex = indices2DCount;
	i32 vertexOffset = (i32)vertices2DCount;

	switch (join) {
		case LINE_JOIN_MITER: {
			u16 first = 0;

			for (u32 i = 0; i < path.subPathCount; i++) {
				struct SubPath2D* p = &path.subPaths[i];

				for (u16 j = 0; j < p->pointCount; j++) {
					vec2 current = p->points[j];
					vec2 previous, next;
					vec2 edge1, edge2;

					if (j != 0 || p->closed) {
						previous = p->points[(j == 0) ? p->pointCount - 1 : j - 1];
						edge1 = vec2Normalize(previous - current);
					}

					if (j != p->pointCount - 1 || p->closed) {
						next = p->points[(j == p->pointCount - 1) ? 0 : j + 1];
						edge2 = vec2Normalize(next - current);
					}

					vec2 miter;
					if (j == 0 && !p->closed)
						miter = (vec2){ edge2.y, -edge2.x };
					else if (j == p->pointCount - 1 && !p->closed)
						miter = (vec2){ -edge1.y, edge1.x };
					else {
						miter = vec2Normalize(edge1 + edge2);
						miter /= vec2Cross(edge1, miter);
					}

					vertices2D[vertices2DCount++] = current + (miter * lineWidth * 0.5f);
					vertices2D[vertices2DCount++] = current - (miter * lineWidth * 0.5f);

					if (j == 0 || p->closed || j < p->pointCount - 1) {
						u16 i0 = first + (j * 2);
						u16 i1 = first + (j * 2) + 1;

						u16 i2, i3;
						if (p->closed && j == p->pointCount - 1) {
							i2 = first;
							i3 = first + 1;
						} else {
							i2 = first + (j * 2) + 2;
							i3 = first + (j * 2) + 3;
						}

						indices2D[indices2DCount++] = i0;
						indices2D[indices2DCount++] = i1;
						indices2D[indices2DCount++] = i2;
						indices2D[indices2DCount++] = i2;
						indices2D[indices2DCount++] = i1;
						indices2D[indices2DCount++] = i3;
					}
				}

				first = (u16)(vertices2DCount - (u32)vertexOffset);
			}
		} break;
		case LINE_JOIN_ROUND: {

		} break;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_PATH2D]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_FRAME].handle, BUFFER_OFFSET_INDICES_2D + frame * BUFFER_RANGE_INDICES_2D, VK_INDEX_TYPE_UINT16);

	vec4 col = (vec4){ color.x, color.y, color.z , color.w } / 255.f;
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(vec4), &col);
	vkCmdDrawIndexed(commandBuffer, indices2DCount - firstIndex, 1, firstIndex, vertexOffset, 0);
}

static inline void drawImage(enum ImageViews imageView, float x, float y, float w, float h) {
	while (vertices2DCount % 4 != 0)
		vertices2DCount++;

	i32 vertexOffset = (i32)vertices2DCount;

	vertices2D[vertices2DCount++] = (vec2){ x, y };
	vertices2D[vertices2DCount++] = (vec2){ x + w, y };
	vertices2D[vertices2DCount++] = (vec2){ x, y + h };
	vertices2D[vertices2DCount++] = (vec2){ x + w, y + h };

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_IMAGE2D]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_DEVICE].handle, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(commandBuffer, 6, 1, BUFFER_OFFSET_QUAD_INDICES / sizeof(u16), vertexOffset, imageView);
}
