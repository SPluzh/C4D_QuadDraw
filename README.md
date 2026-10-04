# C4D QuadDraw — Retopology Tool for Cinema 4D (2026)

An interactive retopology tool for Cinema 4D inspired by Autodesk Maya's **Quad Draw**. It allows artists to quickly create, edit, and relax quad-based geometry over an underlying reference surface or in freeform mode.

https://github.com/user-attachments/assets/7b6e931a-07b8-4054-8844-165ccbf084e4

---

## Features

1. **Editable Mesh and Target Assignment**:
   - Activating the tool with a selected polygon object designates it as the **editable mesh**.
   - A custom **QuadDraw** tag (`Tquaddraw`) is automatically assigned to the editable mesh if not already present.
   - The tag provides a **Target Mesh** link field where you can drop any reference object to enable surface snapping.
   - **No target specified**: the mesh can be edited in freeform mode without snapping (vertices and newly placed points move in camera view plane space).
   - **No selection**: activating the tool without an active mesh automatically creates a new `QuadDraw_Retopo` object with Phong and QuadDraw tags.

2. **Surface Snapping**:
   - Continuous raycast projection onto the target mesh surface (`TargetSurface`).
   - Screen-space snapping to existing retopo vertices with automatic weld on drop.

3. **Edge Extrusion (Extrude Edge)**:
   - **LMB Drag on Border Edge** (or **MMB Drag**): interactively extrudes a new quad polygon from the highlighted border edge along the surface (or in view plane), with real-time surface snapping and automatic vertex welding on drop. The newly extruded outer edge remains highlighted for fast, continuous quad strip creation.
   - Controlled by the **"Extrude Border Edges (LMB Drag)"** checkbox in the tool's Interactive Settings (enabled by default). When unchecked, LMB drag on border edges reverts to standard tweak/move.

4. **Quad Creation**:
   - **Shift + Hover**: previews prospective quad polygons based on nearby vertices.
   - **Shift + LMB**: instantly creates quad polygons with correct surface normal orientation.

5. **Maya-Style Relax Brush**:
   - **Shift + LMB Drag** over faces or edges smooths topology.
   - Multiple relax modes: Auto-Lock (preserves borders or interiors depending on stroke origin), Border Only, Interior Only, or All.
   - **Shift + MMB Drag**: interactively adjusts relax brush radius (horizontal drag) and strength (vertical drag), identical to C4D_RelaxTool.

6. **Tool Modes (Tool Switching in Settings)**:
   - **Active Tool** dropdown in the tool's settings allows switching between:
     - **QuadDraw**: Standard retopology mode for placing points, creating quads, extruding border edges, and tweaking components.
     - **Knife (Cut Loops)**: Dedicated knife tool mode. Hovering over geometry previews edge loop cuts, and **LMB Drag** inserts and slides edge loops directly without needing modifier keys.

7. **Component Loop Highlighting, Drag Extrusion & Moving (Ctrl)**:
   - **Ctrl + Hover**: Hovering over any component highlights its complete loop:
     - Hover over an **Edge** -> highlights the **Edge Loop**.
     - Hover over a **Polygon** -> highlights the **Polygon Loop (Face Loop)** across opposite quad edges.
     - Hover over a **Vertex** -> highlights the **Vertex Loop** passing through that vertex.
   - **Ctrl + LMB (Click)**: Selects the highlighted component loop into Cinema 4D's native selection (`GetWritablePointS`, `GetWritableEdgeS`, or `GetWritablePolygonS`).
   - **Ctrl + LMB Drag** (or **Ctrl + MMB Drag** on edge loops):
     - **Border Edge Loop**: Extrudes the entire boundary edge loop into a continuous strip/ring of quad polygons along the reference surface, complete with snapping and auto-welding.
     - **Interior Edge Loop / Polygon Loop / Vertex Loop**: Interactively translates/tweaks all vertices of the loop across the reference surface in real-time.

8. **Interactive Component Deletion**:
   - **Ctrl + Shift + Hover**: highlights vertices, edge loops, or polygons in red.
   - **Ctrl + Shift + LMB**: removes the highlighted component.

9. **Tweak Mode**:
   - **LMB Drag** on vertices, edges, or polygons moves components directly.
   - Dragging a vertex onto another merges them automatically (Weld).

10. **Display Settings**:
    - **Disable Custom Mesh Shading** (`QUADDRAW_DISABLE_CUSTOM_SHADING`): checked by default to keep clean native Cinema 4D viewport shading on the editable mesh upon tool activation. Uncheck to display translucent retopo polygon face overlays (`Face Color` & `Face Opacity`).
    - **Disable X-Ray (Tools & Shading)** (`QUADDRAW_DISABLE_XRAY`): checked by default to completely prevent vertices, edge cut loops, quad previews, component highlights, and custom shading from being drawn through the mesh (hardware depth testing, Cinema 4D `INVERSE_Z` pass culling, and normal backface culling). Uncheck to see tools and overlays through geometry (X-Ray mode).
    - Configurable wireframe color, line width, vertex point size, and interactive hover colors.

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| **LMB Click** | Place a point on the surface (in Knife mode: insert edge loop cut) |
| **LMB Drag on Border Edge** | Extrude border edge (when setting is enabled; creates new quad, snaps & welds on drop) |
| **LMB Drag** | Tweak/move vertex, interior edge, or polygon (welds on vertex drop; in Knife mode: slides edge loop) |
| **MMB Drag on Edge** | Extrude highlighted border edge (alternative shortcut) |
| **Shift + Hover** | Preview prospective quad polygon |
| **Shift + LMB** | Create quad polygon from preview |
| **Shift + LMB Drag** | Relax topology using the relax brush |
| **Shift + MMB Drag** | Interactively adjust relax brush radius (horizontal) and strength (vertical) |
| **Ctrl + Hover** | Highlight loop of components (Vertex Loop, Edge Loop, or Polygon Loop) |
| **Ctrl + LMB** | Select the highlighted component loop into Cinema 4D selection |
| **Ctrl + Shift + Hover** | Highlight vertex, edge loop, or polygon for deletion |
| **Ctrl + Shift + LMB** | Delete highlighted element |
| **Esc** | Cancel active preview or slide operation |

---

## Build and Installation

- **Cinema 4D 2026**:
  - Build: [build_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.bat) or [build_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.ps1)
  - Deploy: [deploy_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.bat) or [deploy_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.ps1)

- **Cinema 4D 2025**:
  - Build: [build_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.bat) or [build_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.ps1)
  - Deploy: [deploy_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.bat) or [deploy_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.ps1)
