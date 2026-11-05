#include "solvespace.h"

#include "harness.h"

// Fillet and chamfer groups. The models are extrusions of a 40 x 25 mm
// rectangle, 12 mm high, and of an L-shaped section (30 x 10 mm and
// 10 x 10 mm), 15 mm high, followed by a group that rounds or bevels some of
// their straight edges, and a plate with a dome on it (see below), whose
// circular edges are rounded or beveled.

static bool IsWatertight(SMesh *m) {
    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);
    bool noEdges = el.l.IsEmpty();
    el.Clear();
    return noEdges && !inters && !leaks;
}

// All four top edges of the box rounded with a radius of 2 mm. Where two
// rounds meet at a corner, their tools are mitered, and the cylinders meet
// in the bisecting plane, tangent to the top face at one point. Exactly,
// 130 mm of edge lose (1 - pi/4) * 4 mm^2 each, less the overlap at the four
// mitered corners, for 11891.47 mm^3; the mesh approximates the cylinders
// with inscribed facets.
TEST_CASE(round_ring) {
    CHECK_LOAD("round_ring.slvs");
    CHECK_SAVE("round_ring.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->type == Group::Type::FILLET);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 11887.405418, 1.0);
}

// A convex edge of the L rounded, which removes material, and its concave
// inner edge, which adds it. Exactly 6000 - 30 * 0.8584 + 15 * 0.8584 mm^3.
TEST_CASE(round_convex_and_concave) {
    CHECK_LOAD("round_mixed.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5986.819831, 1.0);
}

// The six top edges and the six vertical edges of the L beveled by 2 mm,
// with three bevels meeting at each top corner, one reflex corner on the top
// face, and the concave vertical edge, which gets material added and ends
// just below the bevels of the top edges. All faces are planar, so the mesh
// volume is exact.
TEST_CASE(chamfer_corners) {
    CHECK_LOAD("chamfer.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->subtype == Group::Subtype::EDGE_CHAMFER);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5703.253333, 1.0);
}

// A radius larger than the faces at the edge leaves the solid unchanged and
// reports the edge.
TEST_CASE(too_large) {
    CHECK_LOAD("too_large.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_FALSE(g->edgeError.empty());
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 6000.0, 1.0);
}

// The six top edges of the L rounded with a radius of 2 mm, around the reflex
// corner of the top face, and the concave vertical edge at that corner, which
// ends just below the rounds of the top edges instead of sticking out of them.
TEST_CASE(round_reflex_corner) {
    CHECK_LOAD("round_l.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5926.704136, 1.0);
}

// The dome models are a plate of 30 x 30 x 5 mm with a dome on it, of radius
// 4 mm and 10 mm high, and a hole of radius 1.5 mm in the dome, down to the
// plate. Their circular edges are between a cylinder and a plane face normal
// to its axis, and the blends there are made by revolution.

// The volume swept by the section of a blend of size k, revolved about the
// axis of a circular edge of radius r. The section lies outside the circle
// of the edge or inside it.
static double RevolvedBlend(bool chamfer, double k, double r, bool outside) {
    double area     = chamfer ? k*k/2 : (1 - PI/4)*k*k,
           centroid = chamfer ? k/3 : (10 - 3*PI)/(3*(4 - PI))*k;
    return 2*PI*(r + (outside ? centroid : -centroid))*area;
}

static double MeshVolume(Group *g) {
    g->GenerateDisplayItems();
    return g->displayMesh.CalculateVolume();
}

static Group *Resize(hGroup hg, double size) {
    Group *g = SK.GetGroup(hg);
    g->valA = size;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(hg);
}

// The foot of the dome rounded, which adds material in a ring around it, and
// the top edge of the hole, which removes it. The mesh approximates the
// rounding with facets that lie within the chord tolerance, which is coarse
// against the smaller sizes, so their volume is checked only roughly.
TEST_CASE(round_dome_and_hole) {
    CHECK_LOAD("dome_round.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    hGroup hg = g->h;
    CHECK_TRUE(g->GetEdges().size() == 2);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    double base = MeshVolume(SK.GetGroup(g->opA));
    double v = MeshVolume(g);
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_EQ_EPS(v / 4933.416381, 1.0);

    for(double k : { 0.25, 0.5, 1.2 }) {
        g = Resize(hg, k);
        CHECK_TRUE(g->edgeError.empty());
        v = MeshVolume(g);
        CHECK_TRUE(IsWatertight(&g->displayMesh));
        double exact = RevolvedBlend(false, k, 4.0, true) -
                       RevolvedBlend(false, k, 1.5, true);
        CHECK_TRUE(fabs((v - base) / exact - 1) < 0.1);
    }
}

// The top edge of the dome and the bottom edge of the hole beveled. A size
// larger than the radius of the hole leaves that edge as it was, and reports
// it.
TEST_CASE(chamfer_dome_and_hole) {
    CHECK_LOAD("dome_chamfer.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    hGroup hg = g->h;
    CHECK_TRUE(g->subtype == Group::Subtype::EDGE_CHAMFER);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    double base = MeshVolume(SK.GetGroup(g->opA));
    double v = MeshVolume(g);
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_EQ_EPS(v / 4921.947113, 1.0);

    for(double k : { 0.25, 0.5, 1.0, 1.2 }) {
        g = Resize(hg, k);
        CHECK_TRUE(g->edgeError.empty());
        v = MeshVolume(g);
        CHECK_TRUE(IsWatertight(&g->displayMesh));
        double exact = RevolvedBlend(true, k, 1.5, false) -
                       RevolvedBlend(true, k, 4.0, false);
        CHECK_TRUE(fabs((v - base) / exact - 1) < 0.02);
    }

    g = Resize(hg, 2.0);
    CHECK_FALSE(g->edgeError.empty());
    v = MeshVolume(g);
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_TRUE(fabs((v - base) / -RevolvedBlend(true, 2.0, 4.0, false) - 1) < 0.02);
}

// All four circular edges of a dome whose axis is not parallel to any axis
// of the coordinate system rounded by 0.6 mm.
TEST_CASE(round_tilted_dome) {
    CHECK_LOAD("dome_tilted.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->GetEdges().size() == 4);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    double v = MeshVolume(g);
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_EQ_EPS(v / 3581.065508, 1.0);
}

// The four straight top edges of the plate and the circular foot of the
// dome rounded together.
TEST_CASE(round_straight_and_circular) {
    CHECK_LOAD("dome_mixed.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->GetEdges().size() == 5);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    double v = MeshVolume(g);
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_EQ_EPS(v / 4908.259135, 1.0);
}
