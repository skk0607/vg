#ifndef VG_SNARL_SEED_CLUSTERER_HPP_INCLUDED
#define VG_SNARL_SEED_CLUSTERER_HPP_INCLUDED

#include "snarls.hpp"
#include "snarl_distance_index.hpp"
#include "seed_clusterer.hpp"
#include "hash_map.hpp"
#include "small_bitset.hpp"
#include <structures/union_find.hpp>


namespace vg{


/**
 * NewSnarlSeedClusterer is used for clustering seeds (positions on the graph)
 * A "cluster" is a partition of seeds that is based on the minimum distance between them in the graph
 * Consider a graph where each seed is a node and two seeds are connected if the minimum distance
 * between them is smaller than a given distance limit. Each connected component of this graph is a cluster 
 *
 * The clustering algorithm is based on the snarl tree
 * Clusters are formed on nodes of the snarl tree, which represent nodes/snarls/chains
 * Each node/snarl/chain represents a subgraph of the variation graph
 * A clustered snarl tree node contains all seeds that occur on its subgraph, and the seeds have been partitioned into clusters
 * Each cluster knows the shortest distance from any seed it contains to both ends of the snarl tree node containing it
 * Clustering is done progressively by walking up the snarl tree and forming clusters on each snarl tree node (only visiting nodes that have seeds on them)
 * At each snarl tree node, assume that its children have already been clustered. 
 * The clusters of the children are compared to each other, and any pair that are close enough 
 * are combined to produce clusters on the parent 
 * The distances from each cluster to the ends of the parent are updated
 *
 * The algorithm starts by assigning each seed to its node on the snarl tree
 * Since nodes are all on chains, this fills in all the children of chains that are nodes
 * It then walks up the snarl tree, level by level, and clusters each snarl tree node that contains seeds
 * At a given level, first cluster each chain in the level. After clustering a chain, assign it 
 * to its parent snarl. Then, go through each of the snarls that have just been given children, and
 * cluster the snarls. Each snarl then gets assigned to its parent chain
 * This completes one level of the snarl tree. Each chain in the next level has just been populated by the snarls
 * from this level, and already knew about its nodes from the first step, so it is ready to be clustered 
 *
 * Every time the clusterer is run, a TreeState is made to store information about the state of the clusterer
 * The TreeState keeps track of which level of the snarl tree is currently being clustered, and
 * keeps track of the children of the current and next level of the snarl tree. 
 * 
 * 
 *
 */
class NewSnarlSeedClusterer {



    public:

        /// Seed information used in Giraffe.
        struct Seed {
            pos_t  pos;
            size_t source; // Source minimizer.


            //Cached values from the minimizer
            //(0)record offset of node, (1)record offset of parent, (2)node record offset, (3)node length, (4)is_reversed, 
            // (5)is_trivial_chain, (6)parent is chain, (7)parent is root, (8)prefix sum, (9)chain_component

            tuple<size_t, size_t, size_t, size_t, bool, bool, bool, bool, size_t, size_t> minimizer_cache  = 
                make_tuple(MIPayload::NO_VALUE, MIPayload::NO_VALUE, MIPayload::NO_VALUE, MIPayload::NO_VALUE, false, false, false, false, MIPayload::NO_VALUE, MIPayload::NO_VALUE);

            //The distances to the left and right of whichever cluster this seed represents
            //This gets updated as clustering proceeds
            size_t distance_left = std::numeric_limits<size_t>::max();
            size_t distance_right = std::numeric_limits<size_t>::max();

        };

        /// Cluster information used in Giraffe.
        //struct Cluster {
        //    std::vector<size_t> seeds; // Seed ids.
        //    size_t fragment; // Fragment id.
        //    double score; // Sum of scores of distinct source minimizers of the seeds.
        //    double coverage; // Fraction of read covered by the seeds.
        //    SmallBitset present; // Minimizers that are present in the cluster.
        //};
        typedef SnarlSeedClusterer::Cluster Cluster;

        NewSnarlSeedClusterer(const SnarlDistanceIndex& distance_index, const HandleGraph* graph);
        NewSnarlSeedClusterer(const SnarlDistanceIndex* distance_index, const HandleGraph* graph);


        /*Given a vector of seeds and a distance limit, 
         *cluster the seeds such that two seeds whose minimum distance
         *between them (including both of the positions) is less than
         *the distance limit are in the same cluster
         *This produces a vector of clusters
         */
        vector<Cluster> cluster_seeds ( vector<Seed>& seeds, size_t read_distance_limit) const;
        
        /* The same thing, but for paired end reads.
         * Given seeds from multiple reads of a fragment, cluster each read
         * by the read distance and all seeds by the fragment distance limit.
         * fragment_distance_limit must be greater than read_distance_limit
         * Returns a vector clusters for each read, where each cluster also has an assignment
         * to a fragment cluster
         * Requires that there are only two reads per fragment (all_seeds.size() == 2, meaning paired end reads)
         *    this requirement is just because I used std::pairs to represent two reads, but could be changed to a vector if we every have to map more than two reads per fragment
         */

        vector<vector<Cluster>> cluster_seeds ( 
                vector<vector<Seed>>& all_seeds, size_t read_distance_limit, size_t fragment_distance_limit=0) const;

    private:


        //Actual clustering function that takes a vector of pointers to seeds
        //fragment_distance_limit defaults to 0, meaning that we don't cluster by fragment
        tuple<vector<structures::UnionFind>, structures::UnionFind> cluster_seeds_internal ( 
                vector<vector<Seed>*>& all_seeds,
                size_t read_distance_limit, size_t fragment_distance_limit=0) const;

        const SnarlDistanceIndex& distance_index;
        const HandleGraph* graph;


        /*
         * This struct is used to store the clustering information about one 
         * snarl tree node (node/snarl/chain)
         * It knows the cluster heads of the clusters on the node 
         * and the minimum distance from any seed in each cluster to the ends of the node
         * If the node is a snarl, then the distances stored are to the boundary nodes but
         * don't include the lengths of the boundary nodes; if the node is a node or chain,
         * then the distances include the boundary nodes
         *
         * This also stores additional information about the snarl tree node from the distance index
         * including the distance from the ends of the node to the ends of the parent
         */
        struct NodeClusters {

            //set of the indices of heads of clusters (group ids in the 
            //union find)
            //pair of <read index, seed index>
            hash_set<pair<size_t, size_t>> read_cluster_heads;

            //The shortest distance from any seed in any cluster to the 
            //left/right end of the snarl tree node that contains these
            //clusters
            pair<size_t, size_t> read_best_left = make_pair(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
            pair<size_t, size_t> read_best_right = make_pair(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
            size_t fragment_best_left = std::numeric_limits<size_t>::max();
            size_t fragment_best_right = std::numeric_limits<size_t>::max();

            //Distance from the start of the parent to the left of this node, etc
            size_t distance_start_left = std::numeric_limits<size_t>::max();
            size_t distance_start_right = std::numeric_limits<size_t>::max();
            size_t distance_end_left = std::numeric_limits<size_t>::max();
            size_t distance_end_right = std::numeric_limits<size_t>::max();

            //The snarl tree node that the clusters are on
            net_handle_t containing_net_handle; 
            //THe parent and grandparent of containing_net_handle, which might or might not be set
            //This is just to store information from the minimizer cache
            net_handle_t parent_net_handle;
            net_handle_t grandparent_net_handle;
            //THe boundary node of containing_net_handle, for a snarl or chain
            //if it is a snarl, then this is the actual node, not the sentinel 
            net_handle_t end_in;

            nid_t node_id = 0;

            //Minimum length of a node or snarl
            //If it is a chain, then it is distance_index.chain_minimum_length(), which is
            //the expected length for a normal chain, and the length of the 
            //last component for a multicomponent chain 
            size_t node_length = std::numeric_limits<size_t>::max();             
            size_t prefix_sum_value = std::numeric_limits<size_t>::max(); //of node or first node in snarl
            size_t chain_component_start = 0; //of node or start of snarl
            size_t chain_component_end = 0; //of node or end of snarl

            size_t loop_left = std::numeric_limits<size_t>::max();
            size_t loop_right = std::numeric_limits<size_t>::max();

            //These are sometimes set if the value was in the cache
            bool has_parent_handle = false;
            bool has_grandparent_handle = false;

            //Only set this for nodes or snarls in chains
            bool is_reversed_in_parent = false;

            bool is_trivial_chain = false;
            bool is_looping_chain = false;
            



            //Constructor
            //read_count is the number of reads in a fragment (2 for paired end)
            NodeClusters( net_handle_t net, size_t read_count, size_t seed_count, const SnarlDistanceIndex& distance_index) :
                containing_net_handle(std::move(net)),
                fragment_best_left(std::numeric_limits<size_t>::max()), fragment_best_right(std::numeric_limits<size_t>::max()){
                read_cluster_heads.reserve(seed_count);
            }
            //Constructor for a node or trivial chain, used to remember information from the cache
            NodeClusters( net_handle_t net, size_t read_count, size_t seed_count, bool is_reversed_in_parent, nid_t node_id, size_t node_length, size_t prefix_sum, size_t component) :
                containing_net_handle(net),
                is_reversed_in_parent(is_reversed_in_parent),
                node_length(node_length),
                prefix_sum_value(prefix_sum),
                chain_component_start(component),
                chain_component_end(component),
                node_id(node_id),
                fragment_best_left(std::numeric_limits<size_t>::max()), fragment_best_right(std::numeric_limits<size_t>::max()){
                    read_cluster_heads.reserve(seed_count);
            }

            //Set the values needed to cluster a chain
            void set_chain_values(const SnarlDistanceIndex& distance_index) {
                is_looping_chain = distance_index.is_looping_chain(containing_net_handle);
                node_length = distance_index.chain_minimum_length(containing_net_handle);
                end_in = distance_index.get_bound(containing_net_handle, true, true);
                chain_component_end = distance_index.get_chain_component(end_in, true);
            }

            //Set the values needed to cluster a snarl
            void set_snarl_values(const SnarlDistanceIndex& distance_index) {
                node_length = distance_index.minimum_length(containing_net_handle);
                net_handle_t start_in = distance_index.get_node_from_sentinel(distance_index.get_bound(containing_net_handle, false, true));
                end_in =   distance_index.get_node_from_sentinel(distance_index.get_bound(containing_net_handle, true, true));
                chain_component_start = distance_index.get_chain_component(start_in);
                chain_component_end = distance_index.get_chain_component(end_in);
                prefix_sum_value = SnarlDistanceIndex::sum({
                                 distance_index.get_prefix_sum_value(start_in),
                                 distance_index.minimum_length(start_in)});
                loop_right = SnarlDistanceIndex::sum({distance_index.get_forward_loop_value(end_in),
                                                             2*distance_index.minimum_length(end_in)});
                //Distance to go backward in the chain and back
                loop_left = SnarlDistanceIndex::sum({distance_index.get_reverse_loop_value(start_in),
                                                            2*distance_index.minimum_length(start_in)});


            }

        };

        /*
         * Struct for storing a map from a parent net_handle_t to a list of its children
         * This ended up being used for parent chains, whose children can either be snarls
         * (represented as an index to the snarl's NodeClusters), or seeds
         *
         * The data actually gets stored as a vector of parent, child pairs. To get the 
         * children of a parent, sort the vector and take the range corresponding to the parent
         */
        struct ParentToChildMap {

            //Struct to store one parent-child pair
            struct ParentChildValues {

                //the parent, as an index into all_clusters
                size_t parent_index;

                //The containing net handle of the child
                net_handle_t child_handle;

                //Indices of the child. If it is a snarl, one index into all_clusters and inf. 
                //If it is a seed, the two indices into all_seeds
                size_t child_index1;
                size_t child_index2;
                
                //Chain component of the child
                size_t child_chain_component;
                
                //left offset of the seed or start node of the snarl 
                //(this is used for sorting children of a chain)
                size_t child_offset;

                ParentChildValues(const size_t& parent, const net_handle_t& handle, const size_t& index1, const size_t& index2, const size_t& component, const size_t& offset) :
                    parent_index(parent), child_handle(handle), 
                    child_index1(index1), child_index2(index2),
                    child_chain_component(component), child_offset(offset) {} 
            };

            //This stores the actual data, as parent-child pairs
            //This must be sorted to find the children of a parent
            vector<ParentChildValues> parent_to_children;

            //is parent_to_children sorted?
            //Each time we look up the children of a parent, sort and look it up
            //If it's already sorted, skip sorting
            //Also set this to false any time something gets added
            bool is_sorted = false;

            //Add a child (the handle, child_index) to its parent (parent_index)
            //Component is the component of the node or start component of the snarl
            //Offset is the prefix sum value of the seed, or the prefix sum of the start node of the snarl + 
            // node length of the start node
            // The vector gets unsorted after adding a child
            void add_child(size_t& parent_index, net_handle_t& handle, size_t& child_index, size_t child_index2, 
                           size_t& component, size_t offset) {
                parent_to_children.emplace_back(parent_index, handle, child_index, child_index2, component, offset);
                is_sorted=false;
            }
            void reserve(size_t& size) {
                parent_to_children.reserve(size);
            }

            //Sort the parent_to_children vector first by parent, and second by the order
            //of the children determined by comparator
            void sort(const SnarlDistanceIndex& distance_index) {
                if (!is_sorted) {
                    std::sort(parent_to_children.begin(), parent_to_children.end(),
                    [&] (const ParentChildValues& a,
                         const ParentChildValues& b)->bool {
                        if (a.parent_index == b.parent_index) {
                            //If they are on the same parent chain

                            if (a.child_chain_component == b.child_chain_component) {
                                //If they are on the same component of the chain

                                if (a.child_offset == b.child_offset) {
                                    //If they have the same prefix sum value, order using the distance index
                                    return distance_index.is_ordered_in_chain(a.child_handle, b.child_handle);
                                } else {
                                    //IF they have different prefix sum values, sort by prefix sum
                                    return a.child_offset < b.child_offset;
                                }

                            } else {
                                //If they are on different components, sort by component
                                return a.child_chain_component < b.child_chain_component;
                            }
                        } else {
                            //If they are on different parent chains, sort by parent
                            return a.parent_index < b.parent_index;
                        }
                    });
                    is_sorted = true;
                }
            }

        };

        //These will be the cluster heads and distances for a cluster
        struct ClusterIndices {
            size_t read_num = std::numeric_limits<size_t>::max();
            size_t cluster_num = 0;
            size_t distance_left = 0;
            size_t distance_right = 0;

            ClusterIndices() {}
            ClusterIndices(const size_t& read_num, const size_t& cluster_num, 
                           const size_t& distance_left, const size_t& distance_right) :
                read_num(read_num), cluster_num(cluster_num), 
                distance_left(distance_left), distance_right(distance_right) {} 
        };


        /* Hold all the tree relationships, seed locations, and cluster info
         * for the current level of the snarl tree and the parent level
         * As clustering occurs at the current level, the parent level
         * is updated to know about its children
         *
         * One "level" is the chains at that level, and their parent snarls.
         * Clustering one level means clustering the chains and then clustering the 
         * parent snarls. The parent snarls then get assigned to their parent chains,
         * and TreeState gets reset for the next level (parent chains)
         */
        struct TreeState {

            //Vector of all the seeds for each read
            vector<vector<Seed>*>* all_seeds; 

            //prefix sum vector of the number of seeds per read
            //Used to get the index of a seed for the fragment clusters
            //Also use this so that data structures that store information per seed can be single
            //vectors, instead of a vector of vectors following the structure of all_seeds 
            //since it uses less memory allocation to use a single vector
            vector<size_t> seed_count_prefix_sum;

            //The distance limits.
            //If the minimum distance between two seeds is less than this, 
            //they get put in the same cluster
            size_t read_distance_limit;
            size_t fragment_distance_limit;


            //////////Data structures to hold clustering information

            //Structure to hold the clustering of the seeds
            vector<structures::UnionFind> read_union_find;
            //The indices of seeds in the union find are the indices if you appended each of
            //the vectors of seeds for the fragment (i.e. if a seed in the second read is
            //at index x in the second vector of seeds, then its index in fragment_union_find
            //is x + the length of the first vector of seeds)
            structures::UnionFind fragment_union_find;



            //////////Data structures to hold snarl tree relationships
            //The snarls and chains get updated as we move up the snarl tree

            //Maps each node to a vector of the seeds that are contained in it
            //seeds are represented by indexes into the seeds vector (read_num, seed_num)
            //This only gets used for nodes in the root. All other seeds are added directly
            //to their parent chains as children
            vector<std::tuple<id_t,size_t, size_t>> node_to_seeds;

            //This stores all the node clusters so we stop spending all our time allocating lots of vectors of NodeClusters
            vector<NodeClusters> all_node_clusters;

            //Map each net_handle to its index in all_node_clusters
            hash_map<net_handle_t, size_t> net_handle_to_index;

            
            //Map each chain to its children, which can be snarls or seeds on nodes in the chain. 
            //This is only for the current level of the snarl tree and gets updated as the algorithm
            //moves up the snarl tree. At one iteration, the algorithm will go through each chain
            //in chain to children and cluster the chain using clusters on the children
            ParentToChildMap* chain_to_children;


            //Same structure as chain_to_children but for the level of the snarl
            //tree above the current one
            //This gets updated as the current level is processed - the snarls from this level
            //are added as children to parent_chain_to_children.
            //After processing one level, this becomes the next chain_to_children
            ParentToChildMap* parent_chain_to_children;

            //Map each snarl (as an index into all_node_clusters) to its children (also as an index into all_node_clusters)
            //for the current level of the snarl tree (chains from chain_to_children get added to their parent snarls, 
            //then all snarls in snarl_to_children are clustered and added to parent_chain_to_children)
            std::multimap<size_t, size_t> snarl_to_children;

            //This holds all the child clusters of the root
            //each size_t is the index into all_node_clusters
            //Each pair is the parent and the child. This will be sorted by parent before
            //clustering so it
            vector<pair<size_t, size_t>> root_children;


            /////////////////////////////////////////////////////////

            //Constructor takes in a pointer to the seeds, the distance limits, and 
            //the total number of seeds in all_seeds
            TreeState (vector<vector<Seed>*>* all_seeds, size_t read_distance_limit, 
                       size_t fragment_distance_limit, size_t seed_count) :
                all_seeds(all_seeds),
                read_distance_limit(read_distance_limit),
                fragment_distance_limit(fragment_distance_limit),
                fragment_union_find (seed_count, false),
                seed_count_prefix_sum(1,0){

                for (size_t i = 0 ; i < all_seeds->size() ; i++) {
                    size_t size = all_seeds->at(i)->size();
                    size_t offset = seed_count_prefix_sum.back() + size;
                    seed_count_prefix_sum.push_back(offset);
                    read_union_find.emplace_back(size, false);

                }

                all_node_clusters.reserve(5*seed_count);
                net_handle_to_index.reserve(5*seed_count);
                root_children.reserve(seed_count);
            }
        };

        //Go through all the seeds and assign them to their parent chains or roots
        //If a node is in a chain, then assign it to its parent chain and add the parent
        //chain to chain_to_children_by_level
        //If a node is a child of the root or of a root snarl, then add cluster it and
        //remember to cluster the root snarl 
        void get_nodes( TreeState& tree_state,
                        vector<ParentToChildMap>& chain_to_children_by_level) const;


        //Cluster all the snarls at the current level
        void cluster_snarl_level(TreeState& tree_state) const;

        //Cluster all the chains at the current level
        //also assigns each chain to its parent and saves the distances to the ends of the parent
        //for each chain
        void cluster_chain_level(TreeState& tree_state, size_t depth) const;

        //Cluster the seeds on the specified node
        void cluster_one_node(TreeState& tree_state, NodeClusters& node_clusters) const; 

        //Cluster the seeds in a snarl
        //Snarl_cluster_index is the index into tree_state.all_node_clusters
        //child_range_start/end are the iterators to the start (inclusive) and end (exclusive) 
        //of range of the snarl in snarl_to_children
        void cluster_one_snarl(TreeState& tree_state, size_t snarl_clusters_index, 
                 std::multimap<size_t, size_t>::iterator child_range_start, std::multimap<size_t, size_t>::iterator child_range_end) const;

        //Cluster the seeds in a chain given by chain_clusters_index, an index into
        //distance_index.chain_indexes
        //chain_range_start/end are iterators to the start (inclusive) and end (exclusive) of the
        //range in chain_to_children representing children of this chain
        //The range must be ordered
        //
        //If the children of the chain are only seeds on nodes, then cluster as if it is a node
        void cluster_one_chain(TreeState& tree_state, size_t chain_clusters_index, 
            const std::vector<ParentToChildMap::ParentChildValues>::iterator& chain_range_start,
            const std::vector<ParentToChildMap::ParentChildValues>::iterator& chain_range_end,
            bool only_seeds, bool is_top_level_chain) const;

        //Helper function for adding the next seed to the chain clusters
        void add_seed_to_chain_clusters(TreeState& tree_state, NodeClusters& chain_clusters,
                                        ParentToChildMap::ParentChildValues& last_child,
                                        size_t& last_prefix_sum, size_t& last_length, size_t& last_chain_component_end, 
                                        vector<ClusterIndices>& cluster_heads_to_add_again,
                                        bool& found_first_node, pair<bool, bool>& found_first_node_by_read,
                                        const ParentToChildMap::ParentChildValues& current_child, bool is_first_child, bool is_last_child,
                                        bool skip_distances_to_ends) const;

        //Helper function for adding the next snarl to the chain clusters
        void add_snarl_to_chain_clusters(TreeState& tree_state, NodeClusters& chain_clusters,
                                        ParentToChildMap::ParentChildValues& last_child, 
                                        size_t& last_prefix_sum, size_t& last_length, size_t& last_chain_component_end, 
                                        vector<ClusterIndices>& cluster_heads_to_add_again,
                                        bool& found_first_node, pair<bool, bool>& found_first_node_by_read,
                                        const ParentToChildMap::ParentChildValues& current_child, bool is_first_child, bool is_last_child, 
                                        bool skip_distances_to_ends) const;

        //Cluster in the root - everything in tree_state.root_children 
        void cluster_root(TreeState& tree_state) const;

        //Cluster a list of seeds (SeedIndexes) that are on a single linear structure (node or chain)
        //Requires that the list of seeds are sorted relative to their position on the structure
        //The list of seeds is everything in the list between range_start and range_end
        //This can be called on a chain if there are no nested seeds on the chain
        //get_offset_from_seed_index returns a tuple of <read_num, seed_num, left offset> indices into all_seeds from whatever
        //SeedIndex is used to store the seeds
        //left offset is the distance from the left side of the structure
        template <typename SeedIndex>
        void cluster_seeds_on_linear_structure(TreeState& tree_state, NodeClusters& node_clusters,
                const typename vector<SeedIndex>::iterator& range_start,
                const typename vector<SeedIndex>::iterator& range_end,
                size_t structure_length, std::function<std::tuple<size_t, size_t, size_t>(const SeedIndex&)>& get_offset_from_seed_index, bool skip_distances_to_ends) const;

        //Compare two children of the parent and combine their clusters, to create clusters in the parent
        //child_distances contains the distances for cluster heads in the children, 
        //since the distances in the seeds will get updated to be the distances in the parent
        //First child is true if this is the first time we see child_clusters1. If first_child is true and this is 
        //a snarl, then we need to update the snarl's distances to its parents
        void compare_and_combine_cluster_on_child_structures(TreeState& tree_state, NodeClusters& child_clusters1,
                NodeClusters& child_clusters2, NodeClusters& parent_clusters, 
                const vector<pair<size_t, size_t>>& child_distances, bool is_root, bool first_child) const;

        //The same as above, but compare clusters on a single child
        //This assumes that the child is the child of the root and not a root snarl
        //so we just look at external distances 
        void compare_and_combine_cluster_on_one_child(TreeState& tree_state, NodeClusters& child_clusters) const;

};
}

#endif
