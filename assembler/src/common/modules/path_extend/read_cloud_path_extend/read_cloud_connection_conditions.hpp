#pragma once
#include "common/modules/path_extend/scaffolder2015/connection_condition2015.hpp"
#include "common/modules/path_extend/scaffolder2015/scaffold_graph.hpp"

namespace path_extend {
    //Same as AssemblyGraphConnectionCondition, but stops after reaching unique edges.
    class AssemblyGraphUniqueConnectionCondition : public AssemblyGraphConnectionCondition {
        using AssemblyGraphConnectionCondition::g_;
        using AssemblyGraphConnectionCondition::interesting_edge_set_;
        using AssemblyGraphConnectionCondition::max_connection_length_;
        //fixme duplication with interesting edges, needed to pass to dijkstra
        const ScaffoldingUniqueEdgeStorage& unique_storage_;
     public:
        AssemblyGraphUniqueConnectionCondition(const Graph& g,
                                               size_t max_connection_length,
                                               const ScaffoldingUniqueEdgeStorage& unique_edges);
        map<EdgeId, double> ConnectedWith(EdgeId e) const override;
        virtual bool IsLast() const override;
    };

    class ScaffoldEdgePredicate {
     public:
        typedef scaffold_graph::ScaffoldGraph::ScaffoldEdge ScaffoldEdge;

        virtual bool Check(const scaffold_graph::ScaffoldGraph::ScaffoldEdge& scaffold_edge) const = 0;
        virtual ~ScaffoldEdgePredicate() = default;
    };

    struct LongGapDijkstraParams {
      const size_t barcode_threshold_;
      const size_t count_threshold_;
      const size_t tail_threshold_;
      const size_t len_threshold_;
      const size_t distance_;

      LongGapDijkstraParams(const size_t barcode_threshold_,
                                  const size_t count_threshold_,
                                  const size_t tail_threshold_,
                                  const size_t len_threshold_,
                                  const size_t distance);
    };

    class LongGapDijkstraPredicate: public ScaffoldEdgePredicate {
        using ScaffoldEdgePredicate::ScaffoldEdge;

        const Graph& g;
        const path_extend::ScaffoldingUniqueEdgeStorage& unique_storage_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const LongGapDijkstraParams params_;
     public:
        LongGapDijkstraPredicate(const Graph& g,
                                         const ScaffoldingUniqueEdgeStorage& unique_storage_,
                                         const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_,
                                         const LongGapDijkstraParams& params);
        bool Check(const ScaffoldEdge& scaffold_edge) const override;

    };

    class EdgeSplitPredicate: public ScaffoldEdgePredicate {
        using ScaffoldEdgePredicate::ScaffoldEdge;
        typedef barcode_index::BarcodeId BarcodeId;

        const Graph& g_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const size_t count_threshold_;
        const double strictness_;
     public:
        EdgeSplitPredicate(const Graph& g_,
                           const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_,
                           const size_t count_threshold_,
                           double strictness);

        bool Check(const ScaffoldEdge& scaffold_edge) const override;

     private:
        bool CheckOrderingForThreeSegments(const vector<BarcodeId>& first, const vector<BarcodeId>& second,
                                           const vector<BarcodeId>& third, double strictness) const;

        bool CheckOrderingForFourSegments(const vector<BarcodeId>& first, const vector<BarcodeId>& second,
                                          const vector<BarcodeId>& third, const vector<BarcodeId>& fourth) const;

        DECL_LOGGER("EdgeSplitPredicate");
    };

    class EdgeInTheMiddlePredicate {
     public:
        typedef barcode_index::BarcodeId BarcodeId;

     private:
        const Graph& g_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const size_t count_threshold_;
        const double shared_fraction_threshold_;

     public:
        EdgeInTheMiddlePredicate(const Graph& g_,
                                         const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_,
                                         size_t count_threshold,
                                         double shared_fraction_threshold);

        bool IsCorrectOrdering(const EdgeId& first, const EdgeId& second, const EdgeId& third);
        DECL_LOGGER("EdgeInTheMiddlePredicate");
    };

    class NextFarEdgesPredicate: public ScaffoldEdgePredicate {
     public:
        using ScaffoldEdgePredicate::ScaffoldEdge;
        typedef scaffold_graph::ScaffoldGraph::ScaffoldVertex ScaffoldVertex;

     private:
        const Graph& g_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const size_t count_threshold_;
        const double shared_fraction_threshold_;
        const std::function<vector<ScaffoldVertex>(ScaffoldVertex)>& candidates_getter_;
     public:
        NextFarEdgesPredicate(const Graph& g_,
                              const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_,
                              const size_t count_threshold_,
                              const double shared_fraction_threshold_,
                              const std::function<vector<ScaffoldVertex>(ScaffoldVertex)>& candidates_getter_);

        bool Check(const ScaffoldEdge& scaffold_edge) const override;
    };

    class SimpleSearcher {
     public:
        typedef scaffold_graph::ScaffoldGraph ScaffoldGraph;
        typedef ScaffoldGraph::ScaffoldVertex ScaffoldVertex;
     private:
        const ScaffoldGraph& scaff_graph_;
        const Graph& g_;
        size_t distance_threshold_;

        struct VertexWithDistance {
          ScaffoldVertex vertex;
          size_t distance;
          VertexWithDistance(const ScaffoldVertex& vertex, size_t distance);
        };

     public:
        SimpleSearcher(const scaffold_graph::ScaffoldGraph& graph_, const Graph& g, size_t distance_);

        vector<ScaffoldVertex> GetReachableVertices(const ScaffoldVertex& vertex, const ScaffoldGraph::ScaffoldEdge& restricted_edge);

        void ProcessVertex(std::queue<VertexWithDistance>& vertex_queue, const VertexWithDistance& vertex,
                           std::unordered_set<ScaffoldVertex>& visited, const ScaffoldGraph::ScaffoldEdge& restricted_edge);

        bool AreEqual(const ScaffoldGraph::ScaffoldEdge& first, const ScaffoldGraph::ScaffoldEdge& second);

        DECL_LOGGER("SimpleSearcher");
    };

    class TransitiveEdgesPredicate: public ScaffoldEdgePredicate {
     public:
        using ScaffoldEdgePredicate::ScaffoldEdge;
        typedef scaffold_graph::ScaffoldGraph::ScaffoldVertex ScaffoldVertex;

     private:
        const scaffold_graph::ScaffoldGraph scaffold_graph_;
        const Graph& g_;
        size_t distance_threshold_;
     public:
        TransitiveEdgesPredicate(const scaffold_graph::ScaffoldGraph& graph, const Graph& g, size_t distance_threshold);

        bool Check(const ScaffoldEdge& scaffold_edge) const override;

        DECL_LOGGER("TransitiveEdgesPredicate");
    };

    class EdgePairScoreFunction {
     public:
        virtual double GetScore(const scaffold_graph::ScaffoldGraph::ScaffoldEdge& edge) const = 0;
        virtual ~EdgePairScoreFunction() = default;
    };

    class BarcodeScoreFunction: public EdgePairScoreFunction {
        const size_t read_count_threshold_;
        const size_t tail_threshold_;
        const size_t total_barcodes_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const Graph& graph_;

     public:
        BarcodeScoreFunction(const size_t read_count_threshold,
                             const size_t tail_threshold,
                             const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_,
                             const Graph& graph);

        double GetScore(const scaffold_graph::ScaffoldGraph::ScaffoldEdge& edge) const override;
    };

    class GapCloserPredicate {
     public:
        virtual bool Check(const scaffold_graph::ScaffoldGraph::ScaffoldVertex& Vertex) const = 0;
        virtual ~GapCloserPredicate() = default;
    };

    class LongEdgePairGapCloserPredicate: public GapCloserPredicate {
     public:
        typedef scaffold_graph::ScaffoldGraph ScaffoldGraph;
     private:
        const Graph& g_;
        const barcode_index::FrameBarcodeIndexInfoExtractor& barcode_extractor_;
        const size_t count_threshold_;
        const size_t initial_tail_threshold_;
        const size_t check_tail_threshold_;
        const double raw_score_threshold_;
        const ScaffoldGraph::ScaffoldVertex start_;
        const ScaffoldGraph::ScaffoldVertex end_;
        const vector<barcode_index::BarcodeId> barcodes_;

     public:
        LongEdgePairGapCloserPredicate(const Graph& g, const barcode_index::FrameBarcodeIndexInfoExtractor& extractor,
                                       size_t count_threshold, size_t initial_tail_threshold,
                                       size_t check_tail_threshold, double share_threshold,
                                       const ScaffoldGraph::ScaffoldEdge& edge);

        bool Check(const ScaffoldGraph::ScaffoldVertex& vertex) const override;
        DECL_LOGGER("LongEdgePairGapCloserPredicate");
    };
}
