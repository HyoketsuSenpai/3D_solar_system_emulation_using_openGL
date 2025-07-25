// stb_connected_components - v0.96 - public domain connected components on grids
//                                                 http://github.com/nothings/stb
//
// Finds connected components on 2D grids for testing reachability between
// two points, with fast updates when changing reachability (e.g. on one machine
// it was typically 0.2ms w/ 1024x1024 grid). Each grid square must be "open" or
// "closed" (traversable or untraversable), and grid squares are only connected
// to their orthogonal neighbors, not diagonally.
//
// In one source file, create the implementation by doing something like this:
//
//   #define STBCC_GRID_COUNT_X_LOG2    10
//   #define STBCC_GRID_COUNT_Y_LOG2    10
//   #define STB_CONNECTED_COMPONENTS_IMPLEMENTATION
//   #include "stb_connected_components.h"
//
// The above creates an implementation that can run on maps up to 1024x1024.
// Map sizes must be a multiple of (1<<(LOG2/2)) on each axis (e.g. 32 if LOG2=10,
// 16 if LOG2=8, etc.) (You can just pad your map with untraversable space.)
//
// MEMORY USAGE
//
//   Uses about 6-7 bytes per grid square (e.g. 7MB for a 1024x1024 grid).
//   Uses a single worst-case allocation which you pass in.
//
// PERFORMANCE
//
//   On a core i7-2700K at 3.5 Ghz, for a particular 1024x1024 map (map_03.png):
//
//       Creating map                   : 44.85 ms
//       Making one square   traversable:  0.27 ms    (average over 29,448 calls)
//       Making one square untraversable:  0.23 ms    (average over 30,123 calls)
//       Reachability query:               0.00001 ms (average over 4,000,000 calls)
//
//   On non-degenerate maps update time is O(N^0.5), but on degenerate maps like
//   checkerboards or 50% random, update time is O(N^0.75) (~2ms on above machine).
//
// CHANGELOG
//
//    0.96  (2019-03-04)  Fix warnings
//    0.95  (2016-10-16)  Bugfix if multiple clumps in one cluster connect to same clump in another
//    0.94  (2016-04-17)  Bugfix & optimize worst case (checkerboard & random)
//    0.93  (2016-04-16)  Reduce memory by 10x for 1Kx1K map; small speedup
//    0.92  (2016-04-16)  Compute sqrt(N) cluster size by default
//    0.91  (2016-04-15)  Initial release
//
// TODO:
//    - better API documentation
//    - more comments
//    - try re-integrating naive algorithm & compare performance
//    - more optimized batching (current approach still recomputes local clumps many times)
//    - function for setting a grid of squares at once (just use batching)
//
// LICENSE
//
//   See end of file for license information.
//
// ALGORITHM
//
//   The NxN grid map is split into sqrt(N) x sqrt(N) blocks called
//  "clusters". Each cluster independently computes a set of connected
//   components within that cluster (ignoring all connectivity out of
//   that cluster) using a union-find disjoint set forest. This produces a bunch
//   of locally connected components called "clumps". Each clump is (a) connected
//   within its cluster, (b) does not directly connect to any other clumps in the
//   cluster (though it may connect to them by paths that lead outside the cluster,
//   but those are ignored at this step), and (c) maintains an adjacency list of
//   all clumps in adjacent clusters that it _is_ connected to. Then a second
//   union-find disjoint set forest is used to compute connected clumps
//   globally, across the whole map. Reachability is then computed by
//   finding which clump each input point belongs to, and checking whether
//   those clumps are in the same "global" connected component.
//
//   The above data structure can be updated efficiently; on a change
//   of a single grid square on the map, only one cluster changes its
//   purely-local state, so only one cluster needs its clumps fully
//   recomputed. Clumps in adjacent clusters need their adjacency lists
//   updated: first to remove all references to the old clumps in the
//   rebuilt cluster, then to add new references to the new clumps. Both
//   of these operations can use the existing "find which clump each input
//   point belongs to" query to compute that adjacency information rapidly.

#ifndef INCLUDE_STB_CONNECTED_COMPONENTS_H
#define INCLUDE_STB_CONNECTED_COMPONENTS_H

#include <stdlib.h>

typedef struct st_stbcc_grid stbcc_grid;

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////
//
//  initialization
//

// you allocate the grid data structure to this size (note that it will be very big!!!)
extern size_t stbcc_grid_sizeof(void);

// initialize the grid, value of map[] is 0 = traversable, non-0 is solid
extern void stbcc_init_grid(stbcc_grid *g, unsigned char *map, int w, int h);


//////////////////////////////////////////////////////////////////////////////////////////
//
//  main functionality
//

// update a grid square state, 0 = traversable, non-0 is solid
// i can add a batch-update if it's needed
extern void stbcc_update_grid(stbcc_grid *g, int x, int y, int solid);

// query if two grid squares are reachable from each other
extern int stbcc_query_grid_node_connection(stbcc_grid *g, int x1, int y1, int x2, int y2);


//////////////////////////////////////////////////////////////////////////////////////////
//
//  bonus functions
//

// wrap multiple stbcc_update_grid calls in these function to compute
// multiple updates more efficiently; cannot make queries inside batch
extern void stbcc_update_batch_begin(stbcc_grid *g);
extern void stbcc_update_batch_end(stbcc_grid *g);

// query the grid data structure for whether a given square is open or not
extern int stbcc_query_grid_open(stbcc_grid *g, int x, int y);

// get a unique id for the connected component this is in; it's not necessarily
// small, you'll need a hash table or something to remap it (or just use
extern unsigned int stbcc_get_unique_id(stbcc_grid *g, int x, int y);
#define STBCC_NULL_UNIQUE_ID 0xffffffff // returned for closed map squares

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_STB_CONNECTED_COMPONENTS_H

#ifdef STB_CONNECTED_COMPONENTS_IMPLEMENTATION

#include <assert.h>
#include <string.h> // memset

#if !defined(STBCC_GRID_COUNT_X_LOG2) || !defined(STBCC_GRID_COUNT_Y_LOG2)
   #error "You must define STBCC_GRID_COUNT_X_LOG2 and STBCC_GRID_COUNT_Y_LOG2 to define the max grid supported."
#endif

#define STBCC__GRID_COUNT_X (1 << STBCC_GRID_COUNT_X_LOG2)
#define STBCC__GRID_COUNT_Y (1 << STBCC_GRID_COUNT_Y_LOG2)

#define STBCC__MAP_STRIDE   (1 << (STBCC_GRID_COUNT_X_LOG2-3))

#ifndef STBCC_CLUSTER_SIZE_X_LOG2
   #define STBCC_CLUSTER_SIZE_X_LOG2   (STBCC_GRID_COUNT_X_LOG2/2) // log2(sqrt(2^N)) = 1/2 * log2(2^N)) = 1/2 * N
   #if STBCC_CLUSTER_SIZE_X_LOG2 > 6
   #undef STBCC_CLUSTER_SIZE_X_LOG2
   #define STBCC_CLUSTER_SIZE_X_LOG2 6
   #endif
#endif

#ifndef STBCC_CLUSTER_SIZE_Y_LOG2
   #define STBCC_CLUSTER_SIZE_Y_LOG2   (STBCC_GRID_COUNT_Y_LOG2/2)
   #if STBCC_CLUSTER_SIZE_Y_LOG2 > 6
   #undef STBCC_CLUSTER_SIZE_Y_LOG2
   #define STBCC_CLUSTER_SIZE_Y_LOG2 6
   #endif
#endif

#define STBCC__CLUSTER_SIZE_X   (1 << STBCC_CLUSTER_SIZE_X_LOG2)
#define STBCC__CLUSTER_SIZE_Y   (1 << STBCC_CLUSTER_SIZE_Y_LOG2)

#define STBCC__CLUSTER_COUNT_X_LOG2   (STBCC_GRID_COUNT_X_LOG2 - STBCC_CLUSTER_SIZE_X_LOG2)
#define STBCC__CLUSTER_COUNT_Y_LOG2   (STBCC_GRID_COUNT_Y_LOG2 - STBCC_CLUSTER_SIZE_Y_LOG2)

#define STBCC__CLUSTER_COUNT_X  (1 << STBCC__CLUSTER_COUNT_X_LOG2)
#define STBCC__CLUSTER_COUNT_Y  (1 << STBCC__CLUSTER_COUNT_Y_LOG2)

#if STBCC__CLUSTER_SIZE_X >= STBCC__GRID_COUNT_X || STBCC__CLUSTER_SIZE_Y >= STBCC__GRID_COUNT_Y
   #error "STBCC_CLUSTER_SIZE_X/Y_LOG2 must be smaller than STBCC_GRID_COUNT_X/Y_LOG2"
#endif

// worst case # of clumps per cluster
#define STBCC__MAX_CLUMPS_PER_CLUSTER_LOG2   (STBCC_CLUSTER_SIZE_X_LOG2 + STBCC_CLUSTER_SIZE_Y_LOG2-1)
#define STBCC__MAX_CLUMPS_PER_CLUSTER        (1 << STBCC__MAX_CLUMPS_PER_CLUSTER_LOG2)
#define STBCC__MAX_CLUMPS                    (STBCC__MAX_CLUMPS_PER_CLUSTER * STBCC__CLUSTER_COUNT_X * STBCC__CLUSTER_COUNT_Y)
#define STBCC__NULL_CLUMPID                  STBCC__MAX_CLUMPS_PER_CLUSTER

#define STBCC__CLUSTER_X_FOR_COORD_X(x)  ((x) >> STBCC_CLUSTER_SIZE_X_LOG2)
#define STBCC__CLUSTER_Y_FOR_COORD_Y(y)  ((y) >> STBCC_CLUSTER_SIZE_Y_LOG2)

#define STBCC__MAP_BYTE_MASK(x,y)       (1 << ((x) & 7))
#define STBCC__MAP_BYTE(g,x,y)          ((g)->map[y][(x) >> 3])
#define STBCC__MAP_OPEN(g,x,y)          (STBCC__MAP_BYTE(g,x,y) & STBCC__MAP_BYTE_MASK(x,y))

typedef unsigned short stbcc__clumpid;
typedef unsigned char stbcc__verify_max_clumps[STBCC__MAX_CLUMPS_PER_CLUSTER < (1 << (8*sizeof(stbcc__clumpid))) ? 1 : -1];

#define STBCC__MAX_EXITS_PER_CLUSTER   (STBCC__CLUSTER_SIZE_X + STBCC__CLUSTER_SIZE_Y)   // 64 for 32x32
#define STBCC__MAX_EXITS_PER_CLUMP     (STBCC__CLUSTER_SIZE_X + STBCC__CLUSTER_SIZE_Y)   // 64 for 32x32
#define STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER  (STBCC__MAX_EXITS_PER_CLUMP)

// 2^19 * 2^6 => 2^25 exits => 2^26  => 64MB for 1024x1024

// Logic for above on 4x4 grid:
//
// Many clumps:      One clump:
//   + +               +  +
//  +X.X.             +XX.X+
//   .X.X+             .XXX
//  +X.X.              XXX.
//   .X.X+            +X.XX+
//    + +              +  +
//
// 8 exits either way

typedef unsigned char stbcc__verify_max_exits[STBCC__MAX_EXITS_PER_CLUMP <= 256];

typedef struct
{
   unsigned short clump_index:12;
     signed short cluster_dx:2;
     signed short cluster_dy:2;
} stbcc__relative_clumpid;

typedef union
{
   struct {
      unsigned int clump_index:12;
      unsigned int cluster_x:10;
      unsigned int cluster_y:10;
   } f;
   unsigned int c;
} stbcc__global_clumpid;

// rebuilt cluster 3,4

// what changes in cluster 2,4

typedef struct
{
   stbcc__global_clumpid global_label;        // 4
   unsigned char num_adjacent;                // 1
   unsigned char max_adjacent;                // 1
   unsigned char adjacent_clump_list_index;   // 1
   unsigned char reserved;
} stbcc__clump; // 8

#define STBCC__CLUSTER_ADJACENCY_COUNT   (STBCC__MAX_EXITS_PER_CLUSTER*2)
typedef struct
{
   short num_clumps;
   unsigned char num_edge_clumps;
   unsigned char rebuild_adjacency;
   stbcc__clump clump[STBCC__MAX_CLUMPS_PER_CLUSTER];       // 8 * 2^9 = 4KB
   stbcc__relative_clumpid adjacency_storage[STBCC__CLUSTER_ADJACENCY_COUNT]; // 256 bytes
} stbcc__cluster;

struct st_stbcc_grid
{
   int w,h,cw,ch;
   int in_batched_update;
   //unsigned char cluster_dirty[STBCC__CLUSTER_COUNT_Y][STBCC__CLUSTER_COUNT_X]; // could bitpack, but: 1K x 1K => 1KB
   unsigned char map[STBCC__GRID_COUNT_Y][STBCC__MAP_STRIDE]; // 1K x 1K => 1K x 128 => 128KB
   stbcc__clumpid clump_for_node[STBCC__GRID_COUNT_Y][STBCC__GRID_COUNT_X];  // 1K x 1K x 2 = 2MB
   stbcc__cluster cluster[STBCC__CLUSTER_COUNT_Y][STBCC__CLUSTER_COUNT_X]; //  1K x 4.5KB = 4.5MB
};

/**
 * @brief Determines if two grid cells are reachable from each other.
 *
 * Checks whether the cells at (x1, y1) and (x2, y2) belong to the same connected component of traversable cells.
 *
 * @return 1 if the two cells are connected, 0 otherwise.
 */
int stbcc_query_grid_node_connection(stbcc_grid *g, int x1, int y1, int x2, int y2)
{
   stbcc__global_clumpid label1, label2;
   stbcc__clumpid c1 = g->clump_for_node[y1][x1];
   stbcc__clumpid c2 = g->clump_for_node[y2][x2];
   int cx1 = STBCC__CLUSTER_X_FOR_COORD_X(x1);
   int cy1 = STBCC__CLUSTER_Y_FOR_COORD_Y(y1);
   int cx2 = STBCC__CLUSTER_X_FOR_COORD_X(x2);
   int cy2 = STBCC__CLUSTER_Y_FOR_COORD_Y(y2);
   assert(!g->in_batched_update);
   if (c1 == STBCC__NULL_CLUMPID || c2 == STBCC__NULL_CLUMPID)
      return 0;
   label1 = g->cluster[cy1][cx1].clump[c1].global_label;
   label2 = g->cluster[cy2][cx2].clump[c2].global_label;
   if (label1.c == label2.c)
      return 1;
   return 0;
}

/**
 * @brief Checks if the specified grid cell is traversable.
 *
 * @param x The x-coordinate of the cell.
 * @param y The y-coordinate of the cell.
 * @return Non-zero if the cell at (x, y) is traversable; zero if it is solid.
 */
int stbcc_query_grid_open(stbcc_grid *g, int x, int y)
{
   return STBCC__MAP_OPEN(g, x, y) != 0;
}

/**
 * @brief Returns a unique global identifier for the connected component containing the specified cell.
 *
 * If the cell at (x, y) is traversable, returns a unique ID representing its global connected component. If the cell is solid, returns `STBCC_NULL_UNIQUE_ID`.
 *
 * @param x X-coordinate of the cell.
 * @param y Y-coordinate of the cell.
 * @return Unique global ID for the cell's connected component, or `STBCC_NULL_UNIQUE_ID` if the cell is solid.
 */
unsigned int stbcc_get_unique_id(stbcc_grid *g, int x, int y)
{
   stbcc__clumpid c = g->clump_for_node[y][x];
   int cx = STBCC__CLUSTER_X_FOR_COORD_X(x);
   int cy = STBCC__CLUSTER_Y_FOR_COORD_Y(y);
   assert(!g->in_batched_update);
   if (c == STBCC__NULL_CLUMPID) return STBCC_NULL_UNIQUE_ID;
   return g->cluster[cy][cx].clump[c].global_label.c;
}

typedef struct
{
   unsigned char x,y;
} stbcc__tinypoint;

typedef struct
{
   stbcc__tinypoint parent[STBCC__CLUSTER_SIZE_Y][STBCC__CLUSTER_SIZE_X]; // 32x32 => 2KB
   stbcc__clumpid   label[STBCC__CLUSTER_SIZE_Y][STBCC__CLUSTER_SIZE_X];
} stbcc__cluster_build_info;

static void stbcc__build_clumps_for_cluster(stbcc_grid *g, int cx, int cy);
static void stbcc__remove_connections_to_adjacent_cluster(stbcc_grid *g, int cx, int cy, int dx, int dy);
static void stbcc__add_connections_to_adjacent_cluster(stbcc_grid *g, int cx, int cy, int dx, int dy);

/**
 * @brief Finds the root global label of a clump using path compression.
 *
 * Recursively locates the representative global clump label for the given clump and updates intermediate clumps to point directly to the root for efficiency.
 *
 * @param n The global clump ID to find the root for.
 * @return The root global clump ID representing the connected component.
 */
static stbcc__global_clumpid stbcc__clump_find(stbcc_grid *g, stbcc__global_clumpid n)
{
   stbcc__global_clumpid q;
   stbcc__clump *c = &g->cluster[n.f.cluster_y][n.f.cluster_x].clump[n.f.clump_index];

   if (c->global_label.c == n.c)
      return n;

   q = stbcc__clump_find(g, c->global_label);
   c->global_label = q;
   return q;
}

typedef struct
{
   unsigned int cluster_x;
   unsigned int cluster_y;
   unsigned int clump_index;
} stbcc__unpacked_clumpid;

/**
 * @brief Merges the global connected components of two clumps.
 *
 * Updates the global label of the first clump to match the second, effectively uniting their connected components in the global union-find structure.
 */
static void stbcc__clump_union(stbcc_grid *g, stbcc__unpacked_clumpid m, int x, int y, int idx)
{
   stbcc__clump *mc = &g->cluster[m.cluster_y][m.cluster_x].clump[m.clump_index];
   stbcc__clump *nc = &g->cluster[y][x].clump[idx];
   stbcc__global_clumpid mp = stbcc__clump_find(g, mc->global_label);
   stbcc__global_clumpid np = stbcc__clump_find(g, nc->global_label);

   if (mp.c == np.c)
      return;

   g->cluster[mp.f.cluster_y][mp.f.cluster_x].clump[mp.f.clump_index].global_label = np;
}

/**
 * @brief Computes global connected components for all clumps across the grid.
 *
 * Initializes each clump's global label, then unions clumps that are adjacent across cluster boundaries to form global connected components. Performs path compression to finalize component labels.
 */
static void stbcc__build_connected_components_for_clumps(stbcc_grid *g)
{
   int i,j,k,h;

   for (j=0; j < STBCC__CLUSTER_COUNT_Y; ++j) {
      for (i=0; i < STBCC__CLUSTER_COUNT_X; ++i) {
         stbcc__cluster *cluster = &g->cluster[j][i];
         for (k=0; k < (int) cluster->num_edge_clumps; ++k) {
            stbcc__global_clumpid m;
            m.f.clump_index = k;
            m.f.cluster_x = i;
            m.f.cluster_y = j;
            assert((int) m.f.clump_index == k && (int) m.f.cluster_x == i && (int) m.f.cluster_y == j);
            cluster->clump[k].global_label = m;
         }
      }
   }

   for (j=0; j < STBCC__CLUSTER_COUNT_Y; ++j) {
      for (i=0; i < STBCC__CLUSTER_COUNT_X; ++i) {
         stbcc__cluster *cluster = &g->cluster[j][i];
         for (k=0; k < (int) cluster->num_edge_clumps; ++k) {
            stbcc__clump *clump = &cluster->clump[k];
            stbcc__unpacked_clumpid m;
            stbcc__relative_clumpid *adj;
            m.clump_index = k;
            m.cluster_x = i;
            m.cluster_y = j;
            adj = &cluster->adjacency_storage[clump->adjacent_clump_list_index];
            for (h=0; h < clump->num_adjacent; ++h) {
               unsigned int clump_index = adj[h].clump_index;
               unsigned int x = adj[h].cluster_dx + i;
               unsigned int y = adj[h].cluster_dy + j;
               stbcc__clump_union(g, m, x, y, clump_index);
            }
         }
      }
   }

   for (j=0; j < STBCC__CLUSTER_COUNT_Y; ++j) {
      for (i=0; i < STBCC__CLUSTER_COUNT_X; ++i) {
         stbcc__cluster *cluster = &g->cluster[j][i];
         for (k=0; k < (int) cluster->num_edge_clumps; ++k) {
            stbcc__global_clumpid m;
            m.f.clump_index = k;
            m.f.cluster_x = i;
            m.f.cluster_y = j;
            stbcc__clump_find(g, m);
         }
      }
   }
}

/**
 * @brief Builds adjacency lists for all edge clumps in a cluster.
 *
 * Scans the edges of the specified cluster to identify and record connections between clumps in this cluster and clumps in adjacent clusters. Allocates adjacency slots for each edge clump based on the number of connections and available capacity. Ensures that adjacency information is up to date for efficient global connectivity computation.
 *
 * @param cx X-coordinate of the cluster within the grid.
 * @param cy Y-coordinate of the cluster within the grid.
 */
static void stbcc__build_all_connections_for_cluster(stbcc_grid *g, int cx, int cy)
{
   // in this particular case, we are fully non-incremental. that means we
   // can discover the correct sizes for the arrays, but requires we build
   // the data into temporary data structures, or just count the sizes, so
   // for simplicity we do the latter
   stbcc__cluster *cluster = &g->cluster[cy][cx];
   unsigned char connected[STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER][STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER/8]; // 64 x 8 => 1KB
   unsigned char num_adj[STBCC__MAX_CLUMPS_PER_CLUSTER] = { 0 };
   int x = cx * STBCC__CLUSTER_SIZE_X;
   int y = cy * STBCC__CLUSTER_SIZE_Y;
   int step_x, step_y=0, i, j, k, n, m, dx, dy, total;
   int extra;

   g->cluster[cy][cx].rebuild_adjacency = 0;

   total = 0;
   for (m=0; m < 4; ++m) {
      switch (m) {
         case 0:
            dx = 1, dy = 0;
            step_x = 0, step_y = 1;
            i = STBCC__CLUSTER_SIZE_X-1;
            j = 0;
            n = STBCC__CLUSTER_SIZE_Y;
            break;
         case 1:
            dx = -1, dy = 0;
            i = 0;
            j = 0;
            step_x = 0;
            step_y = 1;
            n = STBCC__CLUSTER_SIZE_Y;
            break;
         case 2:
            dy = -1, dx = 0;
            i = 0;
            j = 0;
            step_x = 1;
            step_y = 0;
            n = STBCC__CLUSTER_SIZE_X;
            break;
         case 3:
            dy = 1, dx = 0;
            i = 0;
            j = STBCC__CLUSTER_SIZE_Y-1;
            step_x = 1;
            step_y = 0;
            n = STBCC__CLUSTER_SIZE_X;
            break;
      }

      if (cx+dx < 0 || cx+dx >= g->cw || cy+dy < 0 || cy+dy >= g->ch)
         continue;

      memset(connected, 0, sizeof(connected));
      for (k=0; k < n; ++k) {
         if (STBCC__MAP_OPEN(g, x+i, y+j) && STBCC__MAP_OPEN(g, x+i+dx, y+j+dy)) {
            stbcc__clumpid src = g->clump_for_node[y+j][x+i];
            stbcc__clumpid dest = g->clump_for_node[y+j+dy][x+i+dx];
            if (0 == (connected[src][dest>>3] & (1 << (dest & 7)))) {
               connected[src][dest>>3] |= 1 << (dest & 7);
               ++num_adj[src];
               ++total;
            }
         }
         i += step_x;
         j += step_y;
      }
   }

   assert(total <= STBCC__CLUSTER_ADJACENCY_COUNT);

   // decide how to apportion unused adjacency slots; only clumps that lie
   // on the edges of the cluster need adjacency slots, so divide them up
   // evenly between those clumps

   // we want:
   //    extra = (STBCC__CLUSTER_ADJACENCY_COUNT - total) / cluster->num_edge_clumps;
   // but we efficiently approximate this without a divide, because
   // ignoring edge-vs-non-edge with 'num_adj[i]*2' was faster than
   // 'num_adj[i]+extra' with the divide
   if      (total + (cluster->num_edge_clumps<<2) <= STBCC__CLUSTER_ADJACENCY_COUNT)
      extra = 4;
   else if (total + (cluster->num_edge_clumps<<1) <= STBCC__CLUSTER_ADJACENCY_COUNT)
      extra = 2;
   else if (total + (cluster->num_edge_clumps<<0) <= STBCC__CLUSTER_ADJACENCY_COUNT)
      extra = 1;
   else
      extra = 0;

   total = 0;
   for (i=0; i < (int) cluster->num_edge_clumps; ++i) {
      int alloc = num_adj[i]+extra;
      if (alloc > STBCC__MAX_EXITS_PER_CLUSTER)
         alloc = STBCC__MAX_EXITS_PER_CLUSTER;
      assert(total < 256); // must fit in byte
      cluster->clump[i].adjacent_clump_list_index = (unsigned char) total;
      cluster->clump[i].max_adjacent = alloc;
      cluster->clump[i].num_adjacent = 0;
      total += alloc;
   }
   assert(total <= STBCC__CLUSTER_ADJACENCY_COUNT);

   stbcc__add_connections_to_adjacent_cluster(g, cx, cy, -1, 0);
   stbcc__add_connections_to_adjacent_cluster(g, cx, cy,  1, 0);
   stbcc__add_connections_to_adjacent_cluster(g, cx, cy,  0,-1);
   stbcc__add_connections_to_adjacent_cluster(g, cx, cy,  0, 1);
   // make sure all of the above succeeded.
   assert(g->cluster[cy][cx].rebuild_adjacency == 0);
}

/**
 * @brief Adds adjacency connections from a cluster to its neighbor and rebuilds adjacency if needed.
 *
 * For the cluster at (cx, cy), adds connections to the adjacent cluster in the (dx, dy) direction.
 * If the cluster's adjacency data is marked for rebuild, recomputes all adjacency connections for that cluster.
 */
static void stbcc__add_connections_to_adjacent_cluster_with_rebuild(stbcc_grid *g, int cx, int cy, int dx, int dy)
{
   if (cx >= 0 && cx < g->cw && cy >= 0 && cy < g->ch) {
      stbcc__add_connections_to_adjacent_cluster(g, cx, cy, dx, dy);
      if (g->cluster[cy][cx].rebuild_adjacency)
         stbcc__build_all_connections_for_cluster(g, cx, cy);
   }
}

/**
 * @brief Updates the traversability state of a single grid cell.
 *
 * Changes the cell at (x, y) to be traversable if `solid` is zero, or solid if non-zero. Recomputes local and global connectivity for the affected cluster and its neighbors. If not in batch update mode, global connected components are rebuilt immediately.
 */
void stbcc_update_grid(stbcc_grid *g, int x, int y, int solid)
{
   int cx,cy;

   if (!solid) {
      if (STBCC__MAP_OPEN(g,x,y))
         return;
   } else {
      if (!STBCC__MAP_OPEN(g,x,y))
         return;
   }

   cx = STBCC__CLUSTER_X_FOR_COORD_X(x);
   cy = STBCC__CLUSTER_Y_FOR_COORD_Y(y);

   stbcc__remove_connections_to_adjacent_cluster(g, cx-1, cy,  1, 0);
   stbcc__remove_connections_to_adjacent_cluster(g, cx+1, cy, -1, 0);
   stbcc__remove_connections_to_adjacent_cluster(g, cx, cy-1,  0, 1);
   stbcc__remove_connections_to_adjacent_cluster(g, cx, cy+1,  0,-1);

   if (!solid)
      STBCC__MAP_BYTE(g,x,y) |= STBCC__MAP_BYTE_MASK(x,y);
   else
      STBCC__MAP_BYTE(g,x,y) &= ~STBCC__MAP_BYTE_MASK(x,y);

   stbcc__build_clumps_for_cluster(g, cx, cy);
   stbcc__build_all_connections_for_cluster(g, cx, cy);

   stbcc__add_connections_to_adjacent_cluster_with_rebuild(g, cx-1, cy,  1, 0);
   stbcc__add_connections_to_adjacent_cluster_with_rebuild(g, cx+1, cy, -1, 0);
   stbcc__add_connections_to_adjacent_cluster_with_rebuild(g, cx, cy-1,  0, 1);
   stbcc__add_connections_to_adjacent_cluster_with_rebuild(g, cx, cy+1,  0,-1);

   if (!g->in_batched_update)
      stbcc__build_connected_components_for_clumps(g);
   #if 0
   else
      g->cluster_dirty[cy][cx] = 1;
   #endif
}

/**
 * @brief Begins a batch update session for the grid.
 *
 * Defers global connected component recomputation until `stbcc_update_batch_end` is called.
 * During a batch, queries are disallowed and multiple cell updates can be performed efficiently.
 */
void stbcc_update_batch_begin(stbcc_grid *g)
{
   assert(!g->in_batched_update);
   g->in_batched_update = 1;
}

/**
 * @brief Ends a batch update and rebuilds global connected components.
 *
 * Call this after a series of cell updates wrapped by stbcc_update_batch_begin(). Recomputes global connectivity for the grid. Queries are allowed again after this call.
 */
void stbcc_update_batch_end(stbcc_grid *g)
{
   assert(g->in_batched_update);
   g->in_batched_update =  0;
   stbcc__build_connected_components_for_clumps(g); // @OPTIMIZE: only do this if update was non-empty
}

/**
 * @brief Returns the size in bytes required to store a grid data structure.
 *
 * @return Size in bytes of the `stbcc_grid` structure.
 */
size_t stbcc_grid_sizeof(void)
{
   return sizeof(stbcc_grid);
}

/**
 * @brief Initializes the grid data structure from a map of traversable and solid cells.
 *
 * Sets up the internal representation of the grid, partitions it into clusters, computes local connected components (clumps) within each cluster, builds adjacency information between clusters, and establishes global connected components for reachability queries. The input map uses 0 for traversable cells and non-zero for solid cells. The grid dimensions must be multiples of the cluster size, and the width must be a multiple of 8.
 *
 * @param g Pointer to the grid data structure to initialize.
 * @param map Pointer to the input map array, row-major, with 0 for traversable and non-zero for solid cells.
 * @param w Width of the grid in cells.
 * @param h Height of the grid in cells.
 */
void stbcc_init_grid(stbcc_grid *g, unsigned char *map, int w, int h)
{
   int i,j,k;
   assert(w % STBCC__CLUSTER_SIZE_X == 0);
   assert(h % STBCC__CLUSTER_SIZE_Y == 0);
   assert(w % 8 == 0);

   g->w = w;
   g->h = h;
   g->cw = w >> STBCC_CLUSTER_SIZE_X_LOG2;
   g->ch = h >> STBCC_CLUSTER_SIZE_Y_LOG2;
   g->in_batched_update = 0;

   #if 0
   for (j=0; j < STBCC__CLUSTER_COUNT_Y; ++j)
      for (i=0; i < STBCC__CLUSTER_COUNT_X; ++i)
         g->cluster_dirty[j][i] = 0;
   #endif

   for (j=0; j < h; ++j) {
      for (i=0; i < w; i += 8) {
         unsigned char c = 0;
         for (k=0; k < 8; ++k)
            if (map[j*w + (i+k)] == 0)
               c |= (1 << k);
         g->map[j][i>>3] = c;
      }
   }

   for (j=0; j < g->ch; ++j)
      for (i=0; i < g->cw; ++i)
         stbcc__build_clumps_for_cluster(g, i, j);

   for (j=0; j < g->ch; ++j)
      for (i=0; i < g->cw; ++i)
         stbcc__build_all_connections_for_cluster(g, i, j);

   stbcc__build_connected_components_for_clumps(g);

   for (j=0; j < g->h; ++j)
      for (i=0; i < g->w; ++i)
         assert(g->clump_for_node[j][i] <= STBCC__NULL_CLUMPID);
}


/**
 * @brief Adds an adjacency connection between clumps in neighboring clusters.
 *
 * Registers a connection from the clump containing cell (x1, y1) to the clump containing cell (x2, y2), where the two cells are in adjacent clusters. If the adjacency list for the clump is full, marks the cluster for adjacency rebuild.
 */
static void stbcc__add_clump_connection(stbcc_grid *g, int x1, int y1, int x2, int y2)
{
   stbcc__cluster *cluster;
   stbcc__clump *clump;

   int cx1 = STBCC__CLUSTER_X_FOR_COORD_X(x1);
   int cy1 = STBCC__CLUSTER_Y_FOR_COORD_Y(y1);
   int cx2 = STBCC__CLUSTER_X_FOR_COORD_X(x2);
   int cy2 = STBCC__CLUSTER_Y_FOR_COORD_Y(y2);

   stbcc__clumpid c1 = g->clump_for_node[y1][x1];
   stbcc__clumpid c2 = g->clump_for_node[y2][x2];

   stbcc__relative_clumpid rc;

   assert(cx1 != cx2 || cy1 != cy2);
   assert(abs(cx1-cx2) + abs(cy1-cy2) == 1);

   // add connection to c2 in c1

   rc.clump_index = c2;
   rc.cluster_dx = x2-x1;
   rc.cluster_dy = y2-y1;

   cluster = &g->cluster[cy1][cx1];
   clump = &cluster->clump[c1];
   assert(clump->num_adjacent <= clump->max_adjacent);
   if (clump->num_adjacent == clump->max_adjacent)
      g->cluster[cy1][cx1].rebuild_adjacency = 1;
   else {
      stbcc__relative_clumpid *adj = &cluster->adjacency_storage[clump->adjacent_clump_list_index];
      assert(clump->num_adjacent < STBCC__MAX_EXITS_PER_CLUMP);
      assert(clump->adjacent_clump_list_index + clump->num_adjacent <= STBCC__CLUSTER_ADJACENCY_COUNT);
      adj[clump->num_adjacent++] = rc;
   }
}

/**
 * @brief Removes the adjacency connection between two clumps in neighboring clusters.
 *
 * Given two grid coordinates in adjacent clusters, removes the adjacency entry from the clump at (x1, y1) that points to the clump at (x2, y2). Asserts if the connection does not exist.
 */
static void stbcc__remove_clump_connection(stbcc_grid *g, int x1, int y1, int x2, int y2)
{
   stbcc__cluster *cluster;
   stbcc__clump *clump;
   stbcc__relative_clumpid *adj;
   int i;

   int cx1 = STBCC__CLUSTER_X_FOR_COORD_X(x1);
   int cy1 = STBCC__CLUSTER_Y_FOR_COORD_Y(y1);
   int cx2 = STBCC__CLUSTER_X_FOR_COORD_X(x2);
   int cy2 = STBCC__CLUSTER_Y_FOR_COORD_Y(y2);

   stbcc__clumpid c1 = g->clump_for_node[y1][x1];
   stbcc__clumpid c2 = g->clump_for_node[y2][x2];

   stbcc__relative_clumpid rc;

   assert(cx1 != cx2 || cy1 != cy2);
   assert(abs(cx1-cx2) + abs(cy1-cy2) == 1);

   // add connection to c2 in c1

   rc.clump_index = c2;
   rc.cluster_dx = x2-x1;
   rc.cluster_dy = y2-y1;

   cluster = &g->cluster[cy1][cx1];
   clump = &cluster->clump[c1];
   adj = &cluster->adjacency_storage[clump->adjacent_clump_list_index];

   for (i=0; i < clump->num_adjacent; ++i)
      if (rc.clump_index == adj[i].clump_index &&
          rc.cluster_dx  == adj[i].cluster_dx  &&
          rc.cluster_dy  == adj[i].cluster_dy)
         break;

   if (i < clump->num_adjacent)
      adj[i] = adj[--clump->num_adjacent];
   else
      assert(0);
}

/**
 * @brief Adds adjacency connections between clumps on the edge of a cluster and its neighboring cluster.
 *
 * For a given cluster at (cx, cy) and a direction (dx, dy), scans the shared edge with the adjacent cluster and adds connections between traversable clumps that are directly connected across the boundary. Skips processing if the cluster is out of bounds or marked for adjacency rebuild.
 *
 * @param cx X index of the source cluster.
 * @param cy Y index of the source cluster.
 * @param dx X offset to the neighboring cluster (must be -1, 0, or 1; only one nonzero).
 * @param dy Y offset to the neighboring cluster (must be -1, 0, or 1; only one nonzero).
 */
static void stbcc__add_connections_to_adjacent_cluster(stbcc_grid *g, int cx, int cy, int dx, int dy)
{
   unsigned char connected[STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER][STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER/8] = { { 0 } };
   int x = cx * STBCC__CLUSTER_SIZE_X;
   int y = cy * STBCC__CLUSTER_SIZE_Y;
   int step_x, step_y=0, i, j, k, n;

   if (cx < 0 || cx >= g->cw || cy < 0 || cy >= g->ch)
      return;

   if (cx+dx < 0 || cx+dx >= g->cw || cy+dy < 0 || cy+dy >= g->ch)
      return;

   if (g->cluster[cy][cx].rebuild_adjacency)
      return;

   assert(abs(dx) + abs(dy) == 1);

   if (dx == 1) {
      i = STBCC__CLUSTER_SIZE_X-1;
      j = 0;
      step_x = 0;
      step_y = 1;
      n = STBCC__CLUSTER_SIZE_Y;
   } else if (dx == -1) {
      i = 0;
      j = 0;
      step_x = 0;
      step_y = 1;
      n = STBCC__CLUSTER_SIZE_Y;
   } else if (dy == -1) {
      i = 0;
      j = 0;
      step_x = 1;
      step_y = 0;
      n = STBCC__CLUSTER_SIZE_X;
   } else if (dy == 1) {
      i = 0;
      j = STBCC__CLUSTER_SIZE_Y-1;
      step_x = 1;
      step_y = 0;
      n = STBCC__CLUSTER_SIZE_X;
   } else {
      assert(0);
      return;
   }

   for (k=0; k < n; ++k) {
      if (STBCC__MAP_OPEN(g, x+i, y+j) && STBCC__MAP_OPEN(g, x+i+dx, y+j+dy)) {
         stbcc__clumpid src = g->clump_for_node[y+j][x+i];
         stbcc__clumpid dest = g->clump_for_node[y+j+dy][x+i+dx];
         if (0 == (connected[src][dest>>3] & (1 << (dest & 7)))) {
            assert((dest>>3) < sizeof(connected));
            connected[src][dest>>3] |= 1 << (dest & 7);
            stbcc__add_clump_connection(g, x+i, y+j, x+i+dx, y+j+dy);
            if (g->cluster[cy][cx].rebuild_adjacency)
               break;
         }
      }
      i += step_x;
      j += step_y;
   }
}

/**
 * @brief Removes adjacency connections between clumps on the edge of a cluster and its neighboring cluster.
 *
 * For the cluster at (cx, cy), removes all adjacency connections to the neighboring cluster in the direction (dx, dy). Only affects clumps connected across the shared edge between the two clusters.
 *
 * Does nothing if the specified cluster or its neighbor is out of bounds.
 */
static void stbcc__remove_connections_to_adjacent_cluster(stbcc_grid *g, int cx, int cy, int dx, int dy)
{
   unsigned char disconnected[STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER][STBCC__MAX_EDGE_CLUMPS_PER_CLUSTER/8] = { { 0 } };
   int x = cx * STBCC__CLUSTER_SIZE_X;
   int y = cy * STBCC__CLUSTER_SIZE_Y;
   int step_x, step_y=0, i, j, k, n;

   if (cx < 0 || cx >= g->cw || cy < 0 || cy >= g->ch)
      return;

   if (cx+dx < 0 || cx+dx >= g->cw || cy+dy < 0 || cy+dy >= g->ch)
      return;

   assert(abs(dx) + abs(dy) == 1);

   if (dx == 1) {
      i = STBCC__CLUSTER_SIZE_X-1;
      j = 0;
      step_x = 0;
      step_y = 1;
      n = STBCC__CLUSTER_SIZE_Y;
   } else if (dx == -1) {
      i = 0;
      j = 0;
      step_x = 0;
      step_y = 1;
      n = STBCC__CLUSTER_SIZE_Y;
   } else if (dy == -1) {
      i = 0;
      j = 0;
      step_x = 1;
      step_y = 0;
      n = STBCC__CLUSTER_SIZE_X;
   } else if (dy == 1) {
      i = 0;
      j = STBCC__CLUSTER_SIZE_Y-1;
      step_x = 1;
      step_y = 0;
      n = STBCC__CLUSTER_SIZE_X;
   } else {
      assert(0);
      return;
   }

   for (k=0; k < n; ++k) {
      if (STBCC__MAP_OPEN(g, x+i, y+j) && STBCC__MAP_OPEN(g, x+i+dx, y+j+dy)) {
         stbcc__clumpid src = g->clump_for_node[y+j][x+i];
         stbcc__clumpid dest = g->clump_for_node[y+j+dy][x+i+dx];
         if (0 == (disconnected[src][dest>>3] & (1 << (dest & 7)))) {
            disconnected[src][dest>>3] |= 1 << (dest & 7);
            stbcc__remove_clump_connection(g, x+i, y+j, x+i+dx, y+j+dy);
         }
      }
      i += step_x;
      j += step_y;
   }
}

/**
 * @brief Finds the root parent of a cell within a cluster using path compression.
 *
 * Performs a union-find operation to locate the representative parent of the cell at (x, y) in the cluster's disjoint set forest, updating parent pointers along the path for efficiency.
 *
 * @param cbi Pointer to the cluster build info containing the parent array.
 * @param x X-coordinate of the cell within the cluster.
 * @param y Y-coordinate of the cell within the cluster.
 * @return The coordinates of the root parent cell representing the connected component.
 */
static stbcc__tinypoint stbcc__incluster_find(stbcc__cluster_build_info *cbi, int x, int y)
{
   stbcc__tinypoint p,q;
   p = cbi->parent[y][x];
   if (p.x == x && p.y == y)
      return p;
   q = stbcc__incluster_find(cbi, p.x, p.y);
   cbi->parent[y][x] = q;
   return q;
}

/**
 * @brief Unions two cells within a cluster in the in-cluster union-find structure.
 *
 * Merges the sets containing the cells at (x1, y1) and (x2, y2) by updating the parent of one root to point to the other.
 */
static void stbcc__incluster_union(stbcc__cluster_build_info *cbi, int x1, int y1, int x2, int y2)
{
   stbcc__tinypoint p = stbcc__incluster_find(cbi, x1,y1);
   stbcc__tinypoint q = stbcc__incluster_find(cbi, x2,y2);

   if (p.x == q.x && p.y == q.y)
      return;

   cbi->parent[p.y][p.x] = q;
}

/**
 * @brief Sets the root of the union-find tree for two cells in a cluster.
 *
 * Updates the parent pointers in the cluster build info so that both the cell at (p.x, p.y) and the cell at (x, y) point to (x, y) as their root.
 *
 * @param cbi Pointer to the cluster build info structure.
 * @param x X-coordinate of the new root cell.
 * @param y Y-coordinate of the new root cell.
 * @param p Coordinates of the other cell to update.
 */
static void stbcc__switch_root(stbcc__cluster_build_info *cbi, int x, int y, stbcc__tinypoint p)
{
   cbi->parent[p.y][p.x].x = x;
   cbi->parent[p.y][p.x].y = y;
   cbi->parent[y][x].x = x;
   cbi->parent[y][x].y = y;
}

/**
 * @brief Builds connected components ("clumps") for a single cluster in the grid.
 *
 * Identifies all orthogonally connected traversable regions within the specified cluster,
 * assigning each a unique clump ID. Edge clumps (those touching the cluster boundary) are
 * labeled first to optimize adjacency computations. Updates the cluster's clump metadata,
 * resets adjacency information, and assigns global labels to interior clumps that cannot
 * connect to other clusters.
 *
 * @param cx X index of the cluster within the grid.
 * @param cy Y index of the cluster within the grid.
 */
static void stbcc__build_clumps_for_cluster(stbcc_grid *g, int cx, int cy)
{
   stbcc__cluster *c;
   stbcc__cluster_build_info cbi;
   int label=0;
   int i,j;
   int x = cx * STBCC__CLUSTER_SIZE_X;
   int y = cy * STBCC__CLUSTER_SIZE_Y;

   // set initial disjoint set forest state
   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j) {
      for (i=0; i < STBCC__CLUSTER_SIZE_X; ++i) {
         cbi.parent[j][i].x = i;
         cbi.parent[j][i].y = j;
      }
   }

   // join all sets that are connected
   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j) {
      // check down only if not on bottom row
      if (j < STBCC__CLUSTER_SIZE_Y-1)
         for (i=0; i < STBCC__CLUSTER_SIZE_X; ++i)
            if (STBCC__MAP_OPEN(g,x+i,y+j) && STBCC__MAP_OPEN(g,x+i  ,y+j+1))
               stbcc__incluster_union(&cbi, i,j, i,j+1);
      // check right for everything but rightmost column
      for (i=0; i < STBCC__CLUSTER_SIZE_X-1; ++i)
         if (STBCC__MAP_OPEN(g,x+i,y+j) && STBCC__MAP_OPEN(g,x+i+1,y+j  ))
            stbcc__incluster_union(&cbi, i,j, i+1,j);
   }

   // label all non-empty clumps along edges so that all edge clumps are first
   // in list; this means in degenerate case we can skip traversing non-edge clumps.
   // because in the first pass we only label leaders, we swap the leader to the
   // edge first

   // first put solid labels on all the edges; these will get overwritten if they're open
   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j)
      cbi.label[j][0] = cbi.label[j][STBCC__CLUSTER_SIZE_X-1] = STBCC__NULL_CLUMPID;
   for (i=0; i < STBCC__CLUSTER_SIZE_X; ++i)
      cbi.label[0][i] = cbi.label[STBCC__CLUSTER_SIZE_Y-1][i] = STBCC__NULL_CLUMPID;

   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j) {
      i = 0;
      if (STBCC__MAP_OPEN(g, x+i, y+j)) {
         stbcc__tinypoint p = stbcc__incluster_find(&cbi, i,j);
         if (p.x == i && p.y == j)
            // if this is the leader, give it a label
            cbi.label[j][i] = label++;
         else if (!(p.x == 0 || p.x == STBCC__CLUSTER_SIZE_X-1 || p.y == 0 || p.y == STBCC__CLUSTER_SIZE_Y-1)) {
            // if leader is in interior, promote this edge node to leader and label
            stbcc__switch_root(&cbi, i, j, p);
            cbi.label[j][i] = label++;
         }
         // else if leader is on edge, do nothing (it'll get labelled when we reach it)
      }
      i = STBCC__CLUSTER_SIZE_X-1;
      if (STBCC__MAP_OPEN(g, x+i, y+j)) {
         stbcc__tinypoint p = stbcc__incluster_find(&cbi, i,j);
         if (p.x == i && p.y == j)
            cbi.label[j][i] = label++;
         else if (!(p.x == 0 || p.x == STBCC__CLUSTER_SIZE_X-1 || p.y == 0 || p.y == STBCC__CLUSTER_SIZE_Y-1)) {
            stbcc__switch_root(&cbi, i, j, p);
            cbi.label[j][i] = label++;
         }
      }
   }

   for (i=1; i < STBCC__CLUSTER_SIZE_Y-1; ++i) {
      j = 0;
      if (STBCC__MAP_OPEN(g, x+i, y+j)) {
         stbcc__tinypoint p = stbcc__incluster_find(&cbi, i,j);
         if (p.x == i && p.y == j)
            cbi.label[j][i] = label++;
         else if (!(p.x == 0 || p.x == STBCC__CLUSTER_SIZE_X-1 || p.y == 0 || p.y == STBCC__CLUSTER_SIZE_Y-1)) {
            stbcc__switch_root(&cbi, i, j, p);
            cbi.label[j][i] = label++;
         }
      }
      j = STBCC__CLUSTER_SIZE_Y-1;
      if (STBCC__MAP_OPEN(g, x+i, y+j)) {
         stbcc__tinypoint p = stbcc__incluster_find(&cbi, i,j);
         if (p.x == i && p.y == j)
            cbi.label[j][i] = label++;
         else if (!(p.x == 0 || p.x == STBCC__CLUSTER_SIZE_X-1 || p.y == 0 || p.y == STBCC__CLUSTER_SIZE_Y-1)) {
            stbcc__switch_root(&cbi, i, j, p);
            cbi.label[j][i] = label++;
         }
      }
   }

   c = &g->cluster[cy][cx];
   c->num_edge_clumps = label;

   // label any internal clusters
   for (j=1; j < STBCC__CLUSTER_SIZE_Y-1; ++j) {
      for (i=1; i < STBCC__CLUSTER_SIZE_X-1; ++i) {
         stbcc__tinypoint p = cbi.parent[j][i];
         if (p.x == i && p.y == j) {
            if (STBCC__MAP_OPEN(g,x+i,y+j))
               cbi.label[j][i] = label++;
            else
               cbi.label[j][i] = STBCC__NULL_CLUMPID;
         }
      }
   }

   // label all other nodes
   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j) {
      for (i=0; i < STBCC__CLUSTER_SIZE_X; ++i) {
         stbcc__tinypoint p = stbcc__incluster_find(&cbi, i,j);
         if (p.x != i || p.y != j) {
            if (STBCC__MAP_OPEN(g,x+i,y+j))
               cbi.label[j][i] = cbi.label[p.y][p.x];
         }
         if (STBCC__MAP_OPEN(g,x+i,y+j))
            assert(cbi.label[j][i] != STBCC__NULL_CLUMPID);
      }
   }

   c->num_clumps = label;

   for (i=0; i < label; ++i) {
      c->clump[i].num_adjacent = 0;
      c->clump[i].max_adjacent = 0;
   }

   for (j=0; j < STBCC__CLUSTER_SIZE_Y; ++j)
      for (i=0; i < STBCC__CLUSTER_SIZE_X; ++i) {
         g->clump_for_node[y+j][x+i] = cbi.label[j][i]; // @OPTIMIZE: remove cbi.label entirely
         assert(g->clump_for_node[y+j][x+i] <= STBCC__NULL_CLUMPID);
      }

   // set the global label for all interior clumps since they can't have connections,
   // so we don't have to do this on the global pass (brings from O(N) to O(N^0.75))
   for (i=(int) c->num_edge_clumps; i < (int) c->num_clumps; ++i) {
      stbcc__global_clumpid gc;
      gc.f.cluster_x = cx;
      gc.f.cluster_y = cy;
      gc.f.clump_index = i;
      c->clump[i].global_label = gc;
   }

   c->rebuild_adjacency = 1; // flag that it has no valid adjacency data
}

#endif // STB_CONNECTED_COMPONENTS_IMPLEMENTATION
/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
