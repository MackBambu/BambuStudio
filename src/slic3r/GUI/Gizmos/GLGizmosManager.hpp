#ifndef slic3r_GUI_GLGizmosManager_hpp_
#define slic3r_GUI_GLGizmosManager_hpp_

#include "slic3r/GUI/GLTexture.hpp"
#include "slic3r/GUI/Gizmos/GLGizmoBase.hpp"
#include "slic3r/GUI/Gizmos/GLGizmosCommon.hpp"
//BBS: GUI refactor: add object manipulation
#include "slic3r/GUI/Gizmos/GizmoObjectManipulation.hpp"

#include "libslic3r/ObjectID.hpp"

#include "wx/timer.h"

#include <map>
#include <memory>
#include <string>

//BBS: GUI refactor: to support top layout
#define BBS_TOOLBAR_ON_TOP 1

namespace Slic3r {

namespace UndoRedo {
struct Snapshot;
}

namespace GUI {

class GLCanvas3D;
class ClippingPlane;
enum class SLAGizmoEventType : unsigned char;
class CommonGizmosDataPool;
//BBS: GUI refactor: add object manipulation
class GizmoObjectManipulation;
class GLToolbar;
class Rect
{
    float m_left;
    float m_top;
    float m_right;
    float m_bottom;

public:
    Rect() : m_left(0.0f) , m_top(0.0f) , m_right(0.0f) , m_bottom(0.0f) {}

    Rect(float left, float top, float right, float bottom) : m_left(left) , m_top(top) , m_right(right) , m_bottom(bottom) {}

    float get_left() const { return m_left; }
    void set_left(float left) { m_left = left; }

    float get_top() const { return m_top; }
    void set_top(float top) { m_top = top; }

    float get_right() const { return m_right; }
    void set_right(float right) { m_right = right; }

    float get_bottom() const { return m_bottom; }
    void set_bottom(float bottom) { m_bottom = bottom; }

    float get_width() const { return m_right - m_left; }
    float get_height() const { return m_top - m_bottom; }
};

class GLGizmosManager : public Slic3r::ObjectBase
{
public:
    static const float Default_Icons_Size;

    enum EType : unsigned char
    {
        // Order must match index in m_gizmos!
        Move,
        Rotate,
        Scale,
        Flatten,
        Cut,
        MeshBoolean,
        Assembly,
        MmuSegmentation,
        Text,
        Svg,
        FdmSupports,
        Seam,
        BrimEars,
        FuzzySkin,
        Measure,
        Simplify,
        SlaSupports,
        // BBS
        //FaceRecognition,
        Hollow,
        Undefined,
    };

private:

    GLCanvas3D& m_parent;
    bool m_enabled;
    std::vector<std::unique_ptr<GLGizmoBase>> m_gizmos;
    EType m_current;
    EType m_hover;
    std::pair<EType, bool> m_highlight; // bool true = higlightedShown, false = highlightedHidden

    //BBS: GUI refactor: add object manipulation
    GizmoObjectManipulation m_object_manipulation;

    std::vector<size_t> get_selectable_idxs() const;

    bool activate_gizmo(EType type);

    bool m_serializing;
    std::unique_ptr<CommonGizmosDataPool> m_common_gizmos_data;

    //When there are more than 9 colors, shortcut key coloring
    wxTimer m_timer_set_color;
    void on_set_color_timer(wxTimerEvent& evt);

    // key MENU_ICON_NAME, value = ImtextureID
    static std::map<int, void*> icon_list;

    bool m_is_dark = false;
public:

    std::unique_ptr<AssembleViewDataPool> m_assemble_view_data;
    enum MENU_ICON_NAME {
        IC_TOOLBAR_RESET            = 0,
        IC_TOOLBAR_RESET_HOVER,
        IC_TOOLBAR_RESET_ZERO,
        IC_TOOLBAR_RESET_ZERO_HOVER,
        IC_TOOLBAR_TOOLTIP,
        IC_TOOLBAR_TOOLTIP_HOVER,
        IC_TEXT_B,
        IC_TEXT_B_DARK,
        IC_TEXT_T,
        IC_TEXT_T_DARK,
        IC_NAME_COUNT,
        IC_FIT_CAMERA,
        IC_FIT_CAMERA_HOVER,
        IC_FIT_CAMERA_DARK,
        IC_FIT_CAMERA_DARK_HOVER,
    };

    explicit GLGizmosManager(GLCanvas3D& parent);

    bool init();

    bool init_icon_textures();

    template<class Archive>
    void load(Archive& ar)
    {
        if (!m_enabled)
            return;

        m_serializing = true;

        // Following is needed to know which to be turn on, but not actually modify
        // m_current prematurely, so activate_gizmo is not confused.
        EType old_current = m_current;
        ar(m_current);
        EType new_current = m_current;
        m_current = old_current;

        // activate_gizmo call sets m_current and calls set_state for the gizmo
        // it does nothing in case the gizmo is already activated
        // it can safely be called for Undefined gizmo
        activate_gizmo(new_current);
        if (m_current != Undefined)
            m_gizmos[m_current]->load(ar);
    }

    template<class Archive>
    void save(Archive& ar) const
    {
        if (!m_enabled)
            return;

        ar(m_current);

        if (m_current != Undefined && !m_gizmos.empty())
            m_gizmos[m_current]->save(ar);
    }

    bool is_enabled() const { return m_enabled; }
    void set_enabled(bool enable) { m_enabled = enable; }

    void refresh_on_off_state();
    void reset_all_states();
    bool is_serializing() const { return m_serializing; }
    bool open_gizmo(EType type);
    bool open_gizmo(unsigned char type);
    bool check_gizmos_closed_except(EType) const;

    void set_hover_id(int id);

    void update(const Linef3& mouse_ray, const Point& mouse_pos);
    void update_data();
    void update_assemble_view_data();

    EType get_current_type() const { return m_current; }
    GLGizmoBase* get_current() const;
    GLGizmoBase *get_gizmo(GLGizmosManager::EType type) const;
    EType get_gizmo_from_name(const std::string& gizmo_name) const;

    bool is_running() const;
    bool handle_shortcut(int key);

    bool is_dragging() const;
    void start_dragging();
    void stop_dragging();

    Vec3d get_displacement() const;

    Vec3d get_scale() const;
    void set_scale(const Vec3d& scale);

    Vec3d get_scale_offset() const;

    Vec3d get_rotation() const;
    void set_rotation(const Vec3d& rotation);

    //BBS
    void* get_icon_texture_id(MENU_ICON_NAME icon) {
        if (icon_list.find((int)icon) != icon_list.end())
            return icon_list[icon];
        else
            return nullptr;
    }
    void* get_icon_texture_id(MENU_ICON_NAME icon) const{
        if (icon_list.find((int)icon) != icon_list.end())
            return icon_list.at(icon);
        else
            return nullptr;
    }
    void  update_paint_base_camera_rotate_rad();
    Vec3d get_flattening_normal() const;

    bool is_gizmo_activable_when_single_full_instance();
    bool is_gizmo_click_empty_not_exit();
    bool is_only_text_volume() const;
    bool is_show_only_active_plate() const;
    bool is_ban_move_glvolume() const;
    bool get_gizmo_active_condition(GLGizmosManager::EType type);
    void update_show_only_active_plate();
    void check_object_located_outside_plate(bool change_plate =true);
    void set_object_located_outside_plate(bool flag) { m_object_located_outside_plate = flag; }
    bool get_object_located_outside_plate() const { return m_object_located_outside_plate; }
    bool gizmo_event(SLAGizmoEventType action, const Vec2d& mouse_position = Vec2d::Zero(), bool shift_down = false, bool alt_down = false, bool control_down = false);
    bool is_paint_gizmo()const;
    bool is_allow_select_all() const;
    bool is_allow_show_volume_highlight_outline() const;
    bool is_allow_drag_volume() const;
    bool is_allow_mouse_drag_selected() const;
    ClippingPlane get_clipping_plane() const;
    ClippingPlane get_assemble_view_clipping_plane() const;
    bool wants_reslice_supports_on_undo() const;

    bool is_in_editing_mode(bool error_notification = false) const;
    bool is_hiding_instances() const;

    void on_change_color_mode(bool is_dark);
    void render_current_gizmo() const;
    void render_current_gizmo_for_picking_pass() const;
    void render_painter_gizmo() const;
    void render_painter_assemble_view() const;

    std::string get_tooltip() const;

    bool on_mouse(wxMouseEvent& evt);
    bool on_mouse_wheel(wxMouseEvent& evt);
    bool on_char(wxKeyEvent& evt);
    bool on_key(wxKeyEvent& evt);

    void update_after_undo_redo(const UndoRedo::Snapshot& snapshot);

    int get_selectable_icons_cnt() const { return get_selectable_idxs().size(); }
    int get_shortcut_key(GLGizmosManager::EType) const;

    // To end highlight set gizmo = undefined
    void set_highlight(EType gizmo, bool highlight_shown);
    bool get_highlight_state() const { return m_highlight.second; }

    GizmoObjectManipulation& get_object_manipulation() { return m_object_manipulation; }
    bool get_uniform_scaling() const { return m_object_manipulation.get_uniform_scaling();}
    BoundingBoxf3 get_bounding_box() const;

    void add_toolbar_items(const std::shared_ptr<GLToolbar>& p_toolbar, uint8_t& sprite_id, const std::function<void(uint8_t& sprite_id)>& p_callback);

    static std::string convert_gizmo_type_to_string(Slic3r::GUI::GLGizmosManager::EType t_type);
private:

    void update_on_off_state(size_t idx);
    bool grabber_contains_mouse() const;
    bool is_text_first_clicked(int idx) const;
    bool is_svg_selected(int idx) const;
    std::string on_hover(int idx);
    void on_click(int idx);

private:
    bool m_object_located_outside_plate{false};
};

} // namespace GUI
} // namespace Slic3r

namespace cereal
{
    template <class Archive> struct specialize<Archive, Slic3r::GUI::GLGizmosManager, cereal::specialization::member_load_save> {};
}

#endif // slic3r_GUI_GLGizmosManager_hpp_
