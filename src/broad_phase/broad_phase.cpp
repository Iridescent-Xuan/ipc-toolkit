#include <ipc/broad_phase/broad_phase.hpp>

#include <ipc/broad_phase/brute_force.hpp>
#include <ipc/broad_phase/spatial_hash.hpp>
#include <ipc/broad_phase/hash_grid.hpp>

namespace ipc {

void build_ignore_vertices(
    size_t num_vertices,
    const Eigen::VectorXi& codim_V,
    const Eigen::MatrixXi& E,
    std::vector<bool>& ignore_vertex)
{
    ignore_vertex.resize(num_vertices, true);
    for (size_t i = 0; i < codim_V.size(); i++) {
        assert(codim_V(i) < num_vertices);
        ignore_vertex[codim_V(i)] = false;
    }
    // Column first because colmajor
    for (size_t ej = 0; ej < E.cols(); ej++) {
        for (size_t ei = 0; ei < E.rows(); ei++) {
            assert(E(ei, ej) < num_vertices);
            ignore_vertex[E(ei, ej)] = false;
        }
    }
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V,
    const Eigen::VectorXi& codim_V,
    bool include_all_vertices,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    int dim = V.cols();

    candidates.clear();

    switch (method) {
    case BroadPhaseMethod::BRUTE_FORCE: {
        std::vector<bool> ignore_vertex;
        if (!include_all_vertices) {
            build_ignore_vertices(V.rows(), codim_V, E, ignore_vertex);
        }
        auto adjusted_can_collide = [&](size_t vi, size_t vj) {
            return (ignore_vertex.empty()
                    || (!ignore_vertex[vi] && !ignore_vertex[vj]))
                && can_collide(vi, vj);
        };
        detect_collision_candidates_brute_force(
            V, E, F, candidates,
            /*detect_edge_vertex=*/dim == 2,
            /*detect_edge_edge=*/dim == 3,
            /*detect_face_vertex=*/dim == 3,
            /*perform_aabb_check=*/true,
            /*aabb_inflation_radius=*/inflation_radius, adjusted_can_collide);
    } break;
    case BroadPhaseMethod::HASH_GRID: {
        HashGrid hash_grid;
        hash_grid.resize(V, E, inflation_radius);

        // Assumes the edges connect to all boundary vertices
        if (include_all_vertices) {
            hash_grid.addVertices(V, inflation_radius);
        } else {
            hash_grid.addVerticesFromEdges(V, E, inflation_radius);
            hash_grid.addSelectVertices(V, codim_V, inflation_radius);
        }
        hash_grid.addEdges(V, E, inflation_radius);
        if (dim == 3) {
            // These are not needed for 2D
            hash_grid.addFaces(V, F, inflation_radius);
        }

        if (dim == 2) {
            // This is not needed for 3D
            hash_grid.getVertexEdgePairs(
                E, candidates.ev_candidates, can_collide);
        } else {
            // These are not needed for 2D
            hash_grid.getEdgeEdgePairs(
                E, candidates.ee_candidates, can_collide);
            hash_grid.getFaceVertexPairs(
                F, candidates.fv_candidates, can_collide);
        }
    } break;
    case BroadPhaseMethod::SPATIAL_HASH: {
        // TODO: Use include_all_vertices and codim_V
        assert(include_all_vertices);
        // TODO: Use can_collide
        SpatialHash sh(V, E, F, inflation_radius);
        sh.queryMeshForCandidates(
            V, E, F, candidates,
            /*queryEV=*/dim == 2,
            /*queryEE=*/dim == 3,
            /*queryFV=*/dim == 3);
    } break;
    }
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    construct_collision_candidates(
        V, /*codim_V=*/Eigen::VectorXi(), /*include_all_vertices=*/true, E, F,
        candidates, inflation_radius, method, can_collide);
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V,
    const Eigen::VectorXi& codim_V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    construct_collision_candidates(
        V, codim_V, /*include_all_vertices=*/false, E, F, candidates,
        inflation_radius, method, can_collide);
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const Eigen::VectorXi& codim_V,
    bool include_all_vertices,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    int dim = V0.cols();
    assert(V1.cols() == dim);

    candidates.clear();

    switch (method) {
    case BroadPhaseMethod::BRUTE_FORCE: {
        std::vector<bool> ignore_vertex;
        if (!include_all_vertices) {
            build_ignore_vertices(V0.rows(), codim_V, E, ignore_vertex);
        }
        auto adjusted_can_collide = [&](size_t vi, size_t vj) {
            return (ignore_vertex.empty()
                    || (!ignore_vertex[vi] && !ignore_vertex[vj]))
                && can_collide(vi, vj);
        };
        detect_collision_candidates_brute_force(
            V0, V1, E, F, candidates,
            /*detect_edge_vertex=*/dim == 2,
            /*detect_edge_edge=*/dim == 3,
            /*detect_face_vertex=*/dim == 3,
            /*perform_aabb_check=*/true,
            /*aabb_inflation_radius=*/inflation_radius, adjusted_can_collide);
    } break;
    case BroadPhaseMethod::HASH_GRID: {
        HashGrid hash_grid;
        hash_grid.resize(V0, V1, E, inflation_radius);

        // Assumes the edges connect to all boundary vertices
        if (include_all_vertices) {
            hash_grid.addVertices(V0, V1, inflation_radius);
        } else {
            hash_grid.addVerticesFromEdges(V0, V1, E, inflation_radius);
            hash_grid.addSelectVertices(V0, V1, codim_V, inflation_radius);
        }
        hash_grid.addEdges(V0, V1, E, inflation_radius);
        if (dim == 3) {
            // These are not needed for 2D
            hash_grid.addFaces(V0, V1, F, inflation_radius);
        }

        if (dim == 2) {
            // This is not needed for 3D
            hash_grid.getVertexEdgePairs(
                E, candidates.ev_candidates, can_collide);
        } else {
            // These are not needed for 2D
            hash_grid.getEdgeEdgePairs(
                E, candidates.ee_candidates, can_collide);
            hash_grid.getFaceVertexPairs(
                F, candidates.fv_candidates, can_collide);
        }
    } break;
    case BroadPhaseMethod::SPATIAL_HASH: {
        // TODO: Use include_all_vertices and codim_V
        assert(include_all_vertices);
        // TODO: Use can_collide
        SpatialHash sh(V0, V1, E, F, inflation_radius);
        sh.queryMeshForCandidates(
            V0, V1, E, F, candidates,
            /*queryEV=*/dim == 2,
            /*queryEE=*/dim == 3,
            /*queryFV=*/dim == 3);
    } break;
    }
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    construct_collision_candidates(
        V0, V1, /*codim_V=*/Eigen::VectorXi(), /*include_all_vertices=*/true, E,
        F, candidates, inflation_radius, method, can_collide);
}

void construct_collision_candidates(
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const Eigen::VectorXi& codim_V,
    const Eigen::MatrixXi& E,
    const Eigen::MatrixXi& F,
    Candidates& candidates,
    double inflation_radius,
    const BroadPhaseMethod& method,
    const std::function<bool(size_t, size_t)>& can_collide)
{
    construct_collision_candidates(
        V0, V1, codim_V, /*include_all_vertices=*/false, E, F, candidates,
        inflation_radius, method, can_collide);
}

} // namespace ipc
