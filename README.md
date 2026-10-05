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
     - **Extrude**: Standard retopology mode for placing points, creating quads, extruding border edges, and tweaking components.
     - **Move / Tweak**: Dedicated transform and move mode for tweaking vertices, edges, border edges (without triggering extrusion), polygons, and component loops with real-time surface snapping and auto-welding (creates points on empty surface click just like Extrude).
     - **Knife (Cut Loops)**: Dedicated knife tool mode. Hovering over geometry previews edge loop cuts, and **LMB Drag** inserts and slides edge loops directly without needing modifier keys.
     - **Multi-Cut**: Autodesk Maya-style Multi-Cut tool. Point-to-point polygon cutting and screen slice cutting:
       - **Point-to-Point Cut**: **LMB Click** snaps cut points onto edges (slides with hover) or vertices. Chains cut points across quads and triangles.
       - **Snapping Increments**: Holding **Shift** snaps the cut point along edges to 50% midpoint and 10%/25% intervals.
       - **Commit Cut**: Press **Enter**, **RMB (Right Mouse Button)**, or **Double-Click** to finalize the cut and split the polygons.
       - **Undo Point**: Press **Backspace** or **Delete** to step back and remove the last placed point.
       - **Slice Cut**: **LMB Drag** across the mesh/screen draws a slice line that cuts through all intersected faces.
       - **Edge Loops with Ctrl**: Holding **Ctrl** in Multi-Cut mode previews and inserts edge loops (identical to Maya Multi-Cut).
      - **Delete**: Dedicated component deletion tool:
        - **LMB Click**: Deletes single hovered vertex (including free unconnected dots), edge, or polygon. Deleting an unconnected point removes only that specific point and preserves all other unconnected points.
        - **Ctrl + LMB Click**: Deletes the entire component chain/loop (Edge Loop, Polygon Loop, or Vertex Loop).
      - **Delete All Unconnected Points Button**: A dedicated button in the **Tool Mode** settings to instantly clean up all isolated/free dots that are not connected to any polygon.

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
   - **Dedicated Delete Tool**: Switch to **Delete** mode in settings. Hover over any vertex, edge, or polygon to highlight it in red and click **LMB** to delete it. Hold **Ctrl** (**Ctrl + LMB**) to delete the entire chain/loop (Edge Loop, Polygon Loop, or Vertex Loop).

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
| **LMB Click** | Place a point on surface (in Knife: insert edge loop; in Multi-Cut: place cut point on edge/vertex) |
| **LMB Drag on Border Edge** | Extrude border edge (when setting is enabled; creates new quad, snaps & welds on drop) |
| **LMB Drag** | Tweak/move component (welds on drop; in Knife: slide edge loop; in Multi-Cut: slice cut across faces) |
| **MMB Drag on Edge** | Extrude highlighted border edge (alternative shortcut) |
| **Shift + Hover** | Preview prospective quad polygon (in Multi-Cut: snap cut point to 50% midpoint and 10%/25% steps) |
| **Shift + LMB** | Create quad polygon from preview |
| **Shift + LMB Drag** | Relax topology using the relax brush |
| **Shift + MMB Drag** | Interactively adjust relax brush radius (horizontal) and strength (vertical) |
| **Enter / RMB / Double-Click** | Commit Multi-Cut cut (splits quads/triangles along cut chain) |
| **Backspace / Delete** | Remove last placed point in Multi-Cut |
| **Ctrl + Hover** | Highlight loop of components (in Multi-Cut: preview edge loop cut) |
| **Ctrl + LMB** | Select component loop into selection (in Multi-Cut: insert edge loop) |
| **Ctrl + Shift + Hover** | Highlight vertex, edge loop, or polygon for deletion |
| **Ctrl + Shift + LMB** | Delete highlighted element |
| **Esc** | Cancel active preview, slide operation, or multi-cut chain |

---

## Build and Installation

- **Cinema 4D 2026**:
  - Build: [build_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.bat) or [build_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.ps1)
  - Deploy: [deploy_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.bat) or [deploy_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.ps1)

- **Cinema 4D 2025**:
  - Build: [build_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.bat) or [build_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.ps1)
  - Deploy: [deploy_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.bat) or [deploy_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.ps1)
