#ifndef slic3r_FillBase_hpp_
#define slic3r_FillBase_hpp_

#include <assert.h>
#include <memory.h>
#include <float.h>
#include <stdint.h>
#include <stdexcept>

#include <type_traits>

#include "../libslic3r.h"
#include "../BoundingBox.hpp"
#include "../Exception.hpp"
#include "../Utils.hpp"
#include "../ExPolygon.hpp"
//BBS: necessary header for new function
#include "../PrintConfig.hpp"
#include "../Flow.hpp"
#include "../ExtrusionEntity.hpp"
#include "../ExtrusionEntityCollection.hpp"

namespace Slic3r {

class Surface;
enum InfillPattern : int;

namespace FillAdaptive {
    struct Octree;
};

// Infill shall never fail, therefore the error is classified as RuntimeError, not SlicingError.
class InfillFailedException : public Slic3r::RuntimeError {
public:
    InfillFailedException() : Slic3r::RuntimeError("Infill failed") {}
};

struct LockRegionParam
{
    LockRegionParam() {}
    std::map<float, ExPolygons> skin_density_params;
    std::map<float, ExPolygons> skeleton_density_params;
    std::map<Flow, ExPolygons>  skin_flow_params;
    std::map<Flow, ExPolygons>  skeleton_flow_params;
};

struct FillParams
{
    bool        full_infill() const { return density > 0.9999f; }
    // Don't connect the fill lines around the inner perimeter.
    bool        dont_connect() const { return anchor_length_max < 0.05f; }
    double      filter_out_gap_fill { 0.0 };
    // Fill density, fraction in <0, 1>
    float       density 		{ 0.f };

    // Length of an infill anchor along the perimeter.
    // 1000mm is roughly the maximum length line that fits into a 32bit coord_t.
    float       anchor_length       { 1000.f };
    float       anchor_length_max   { 1000.f };

    // G-code resolution.
    double      resolution          { 0.0125 };

    // Don't adjust spacing to fill the space evenly.
    bool        dont_adjust 	{ true };

    // Monotonic infill - strictly left to right for better surface quality of top infills.
    bool 		monotonic		{ false };

    // For Honeycomb.
    // we were requested to complete each loop;
    // in this case we don't try to make more continuous paths
    bool        complete 		{ false };

    // For Concentric infill, to switch between Classic and Arachne.
    bool        use_arachne{ false };
    // Layer height for Concentric infill with Arachne.
    coordf_t    layer_height    { 0.f };

    // BBS
    Flow            flow;
    ExtrusionRole   extrusion_role{ ExtrusionRole(0) };
    bool            using_internal_flow{ false };
    //BBS: only used for new top surface pattern
    float           no_extrusion_overlap{ 0.0 };
    bool            dont_sort{ false }; // do not sort the lines, just simply connect them
    bool            can_reverse{true};

    float           horiz_move{0.0}; //move infill to get cross zag pattern
    bool            symmetric_infill_y_axis{false};
    coord_t         symmetric_y_axis{0};
    bool            locked_zag{false};
    float           infill_lock_depth{0.0};
    float           skin_infill_depth{0.0};
};
static_assert(IsTriviallyCopyable<FillParams>::value, "FillParams class is not POD (and it should be - see constructor).");

class Fill
{
public:
    // Index of the layer.
    size_t      layer_id;
    // Z coordinate of the top print surface, in unscaled coordinates
    coordf_t    z;
    // in unscaled coordinates
    coordf_t    spacing;
    // infill / perimeter overlap, in unscaled coordinates
    coordf_t    overlap;
    // in radians, ccw, 0 = East
    float       angle;
    // In scaled coordinates. Maximum lenght of a perimeter segment connecting two infill lines.
    // Used by the FillRectilinear2, FillGrid2, FillTriangles, FillStars and FillCubic.
    // If left to zero, the links will not be limited.
    coord_t     link_max_length;
    // In scaled coordinates. Used by the concentric infill pattern to clip the loops to create extrusion paths.
    coord_t     loop_clipping;
    // In scaled coordinates. Bounding box of the 2D projection of the object.
    BoundingBox bounding_box;

    // Octree builds on mesh for usage in the adaptive cubic infill
    FillAdaptive::Octree* adapt_fill_octree = nullptr;

    // BBS: all no overlap expolygons in same layer
    ExPolygons  no_overlap_expolygons;

public:
    virtual ~Fill() {}
    virtual Fill* clone() const = 0;

    static Fill* new_from_type(const InfillPattern type);
    static Fill* new_from_type(const std::string &type);
    static bool  use_bridge_flow(const InfillPattern type);

    void         set_bounding_box(const Slic3r::BoundingBox &bbox) { bounding_box = bbox; }
    BoundingBox  extended_object_bounding_box() const;
    // Use bridge flow for the fill?
    virtual bool use_bridge_flow() const { return false; }

    // Do not sort the fill lines to optimize the print head path?
    virtual bool no_sort() const { return false; }

    virtual bool is_self_crossing() = 0;

    // Return true if infill has a consistent pattern between layers.
    virtual bool has_consistent_pattern() const { return false; }

    // Perform the fill.
    virtual Polylines fill_surface(const Surface *surface, const FillParams &params);
    virtual ThickPolylines fill_surface_arachne(const Surface* surface, const FillParams& params);
    virtual void set_lock_region_param(const LockRegionParam &lock_param){};
    virtual void set_skin_and_skeleton_pattern(const InfillPattern &skin_pattern, const InfillPattern &skeleton_pattern){};
    // BBS: this method is used to fill the ExtrusionEntityCollection.
    // It call fill_surface by default
    virtual void fill_surface_extrusion(const Surface *surface, const FillParams &params, ExtrusionEntitiesPtr &out);
    virtual void copy_fill_data(const Fill *f){
        layer_id              = f->layer_id;
        z                     = f->z;
        spacing               = f->spacing;
        overlap               = f->overlap;
        angle                 = f->angle;
        link_max_length       = f->link_max_length;
        loop_clipping         = f->loop_clipping;
        bounding_box          = f->bounding_box;
        adapt_fill_octree     = f->adapt_fill_octree;
        no_overlap_expolygons = f->no_overlap_expolygons;
    };

protected:
    Fill() :
        layer_id(size_t(-1)),
        z(0.),
        spacing(0.),
        // Infill / perimeter overlap.
        overlap(0.),
        // Initial angle is undefined.
        angle(FLT_MAX),
        link_max_length(0),
        loop_clipping(0),
        // The initial bounding box is empty, therefore undefined.
        bounding_box(Point(0, 0), Point(-1, -1))
        {}

    // The expolygon may be modified by the method to avoid a copy.
    virtual void    _fill_surface_single(
        const FillParams                & /* params */,
        unsigned int                      /* thickness_layers */,
        const std::pair<float, Point>   & /* direction */,
        ExPolygon                         /* expolygon */,
        Polylines                       & /* polylines_out */) {}

    // Used for concentric infill to generate ThickPolylines using Arachne.
    virtual void _fill_surface_single(const FillParams& params,
        unsigned int                   thickness_layers,
        const std::pair<float, Point>& direction,
        ExPolygon                      expolygon,
        ThickPolylines& thick_polylines_out) {}

    virtual float _layer_angle(size_t idx) const { return (idx & 1) ? float(M_PI/2.) : 0; }

    virtual std::pair<float, Point> _infill_direction(const Surface *surface) const;

public:
    static void connect_infill(Polylines &&infill_ordered, const ExPolygon &boundary, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_infill(Polylines &&infill_ordered, const Polygons &boundary, const BoundingBox& bbox, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_infill(Polylines &&infill_ordered, const std::vector<const Polygon*> &boundary, const BoundingBox &bbox, Polylines &polylines_out, double spacing, const FillParams &params);

    static void connect_base_support(Polylines &&infill_ordered, const std::vector<const Polygon*> &boundary_src, const BoundingBox &bbox, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_base_support(Polylines &&infill_ordered, const Polygons &boundary_src, const BoundingBox &bbox, Polylines &polylines_out, const double spacing, const FillParams &params);

    static coord_t  _adjust_solid_spacing(const coord_t width, const coord_t distance);
};

} // namespace Slic3r

#endif // slic3r_FillBase_hpp_
