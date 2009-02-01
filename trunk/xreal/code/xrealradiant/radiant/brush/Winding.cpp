/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Winding.h"
#include "igl.h"
#include <algorithm>
#include "FixedWinding.h"
#include "math/line.h"
#include "math/Plane3.h"
#include "texturelib.h"
#include "Brush.h"

namespace {
	struct indexremap_t {
		indexremap_t(std::size_t _x, std::size_t _y, std::size_t _z) :
			x(_x), y(_y), z(_z)
		{}
		
		std::size_t x, y, z;
	};
	
	inline indexremap_t indexremap_for_projectionaxis(const ProjectionAxis axis) {
		switch (axis) {
			case eProjectionAxisX:
				return indexremap_t(1, 2, 0);
			case eProjectionAxisY:
				return indexremap_t(2, 0, 1);
			default:
				return indexremap_t(0, 1, 2);
		}
	}
}

void Winding::drawWireframe() const {
	if (points.size() > 0) {
		glVertexPointer(3, GL_DOUBLE, sizeof(WindingVertex), &points.front().vertex);
		glDrawArrays(GL_LINE_LOOP, 0, GLsizei(numpoints));
	}
}

void Winding::draw(RenderStateFlags state) const {
	if (points.size() == 0) {
		return;
	}

	// A shortcut pointer to the first array element to avoid
	// massive calls to std::vector<>::begin()
	const WindingVertex& firstElement = points.front();
		
	// Set the vertex pointer first
	glVertexPointer(3, GL_DOUBLE, sizeof(WindingVertex), &firstElement.vertex);

	if ((state & RENDER_BUMP) != 0) {
		glVertexAttribPointerARB(11, 3, GL_DOUBLE, 0, sizeof(WindingVertex), &firstElement.normal);
		glVertexAttribPointerARB(8, 2, GL_DOUBLE, 0, sizeof(WindingVertex), &firstElement.texcoord);
		glVertexAttribPointerARB(9, 3, GL_DOUBLE, 0, sizeof(WindingVertex), &firstElement.tangent);
		glVertexAttribPointerARB(10, 3, GL_DOUBLE, 0, sizeof(WindingVertex), &firstElement.bitangent);
	} 
	else {
		if (state & RENDER_LIGHTING) {
			glNormalPointer(GL_DOUBLE, sizeof(WindingVertex), &firstElement.normal);
		}

		if (state & RENDER_TEXTURE) {
			glTexCoordPointer(2, GL_DOUBLE, sizeof(WindingVertex), &firstElement.texcoord);
		}
	}
	
	glDrawArrays(GL_POLYGON, 0, GLsizei(numpoints));
}

void Winding::testSelect(SelectionTest& test, SelectionIntersection& best) {
	if (numpoints > 0) {
		test.TestPolygon(VertexPointer(reinterpret_cast<VertexPointer::pointer>(&points.front().vertex), sizeof(WindingVertex)), numpoints, best);
	}
}

void Winding::updateNormals(const Vector3& normal) {
	// Copy all normals into the winding vertices
	for (iterator i = begin(); i != end(); i++) {
		i->normal = normal;
	}
}

AABB Winding::aabb() const {
	AABB returnValue;
	
	for (const_iterator i = begin(); i != end(); i++) {
		returnValue.includePoint(i->vertex);
	}
	
	return returnValue;
}

bool Winding::testPlane(const Plane3& plane, bool flipped) const {
	const int test = (flipped) ? ePlaneBack : ePlaneFront;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		if (test == classifyDistance(plane.distanceToPoint(i->vertex), ON_EPSILON)) {
			return false;
		}
	}

	return true;
}

BrushSplitType Winding::classifyPlane(const Plane3& plane) const {
	BrushSplitType split;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		++split.counts[classifyDistance(plane.distanceToPoint(i->vertex), ON_EPSILON)];
	}
	
	return split;
}

PlaneClassification Winding::classifyDistance(const double distance, const double epsilon) {
	if (distance > epsilon) {
		return ePlaneFront;
	}
	
	if (distance < -epsilon) {
		return ePlaneBack;
	}
	
	return ePlaneOn;
}

bool Winding::planesConcave(const Winding& w1, const Winding& w2, const Plane3& plane1, const Plane3& plane2) {
	return !w1.testPlane(plane2, false) || !w2.testPlane(plane1, false);
}

std::size_t Winding::findAdjacent(std::size_t face) const {
	for (std::size_t i = 0; i < numpoints; ++i) {
		ASSERT_MESSAGE((*this)[i].adjacent != c_brush_maxFaces, "edge connectivity data is invalid");
		if ((*this)[i].adjacent == face) {
			return i;
		}
	}
	
	return c_brush_maxFaces;
}

std::size_t Winding::opposite(const std::size_t index, const std::size_t other) const {
	ASSERT_MESSAGE(index < numpoints && other < numpoints, "Winding::opposite: index out of range");
	
	double dist_best = 0;
	std::size_t index_best = c_brush_maxFaces;

	Ray edge(ray_for_points((*this)[index].vertex, (*this)[other].vertex));

	for (std::size_t i=0; i < numpoints; ++i) {
		if (i == index || i == other) {
			continue;
		}

		double dist_squared = ray_squared_distance_to_point(edge, (*this)[i].vertex);

		if (dist_squared > dist_best) {
			dist_best = dist_squared;
			index_best = i;
		}
	}
	
	return index_best;
}

std::size_t Winding::opposite(std::size_t index) const {
	return opposite(index, next(index));
}
	
Vector3 Winding::centroid(const Plane3& plane) const {
	Vector3 centroid(0,0,0);
	
	double area2 = 0, x_sum = 0, y_sum = 0;
	const ProjectionAxis axis = projectionaxis_for_normal(plane.normal());
	const indexremap_t remap = indexremap_for_projectionaxis(axis);
	
	for (std::size_t i = numpoints - 1, j = 0; j < numpoints; i	= j, ++j) {
		const double ai = (*this)[i].vertex[remap.x]
				* (*this)[j].vertex[remap.y] - (*this)[j].vertex[remap.x]
				* (*this)[i].vertex[remap.y];
		
		area2 += ai;
		
		x_sum += ((*this)[j].vertex[remap.x] + (*this)[i].vertex[remap.x]) * ai;
		y_sum += ((*this)[j].vertex[remap.y] + (*this)[i].vertex[remap.y]) * ai;
	}

	centroid[remap.x] = x_sum / (3 * area2);
	centroid[remap.y] = y_sum / (3 * area2);
	{
		Ray ray(Vector3(0, 0, 0), Vector3(0, 0, 0));
		ray.origin[remap.x] = centroid[remap.x];
		ray.origin[remap.y] = centroid[remap.y];
		ray.direction[remap.z] = 1;
		centroid[remap.z] = ray_distance_to_plane(ray, plane);
	}
	
	return centroid;
}

void Winding::printConnectivity() {
	for (iterator i = begin(); i != end(); ++i) {
		std::size_t vertexIndex = std::distance(begin(), i);
		globalOutputStream() << "vertex: " << Unsigned(vertexIndex)
			<< " adjacent: " << Unsigned(i->adjacent) << "\n";
	}
}