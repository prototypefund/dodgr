#include "pathfinders.h"
#include "heaps/heap.h"

#include <algorithm> // std::fill

#include <Rcpp.h> // TODO: Delete!

// Modified from code by Shane Saunders

// @param n Number of vertices in graph
// @param heapD The type of heap used
// @param g A DGraph object
// @param twoheap If `TRUE`, uses a bi-directional search.
PathFinder::PathFinder(unsigned int n,
        const HeapDesc& heapD,
        std::shared_ptr<const DGraph> g,
        bool twoheap) 
{
    m_heap = heapD.newInstance(n);
    m_closed = new bool[n];
    m_open = new bool[n];
    _twoheap = twoheap;
    if (_twoheap)
    {
        m_heap_rev = heapD.newInstance(n);
        m_open2 = new bool[n];
        m_closed2 = new bool[n];
    }
    init(g);
}

PathFinder::~PathFinder() {
    delete [] m_open;
    if (_twoheap)
    {
        delete [] m_open2;
        delete [] m_closed;
        delete [] m_closed2;
        delete m_heap_rev;
    }
    delete m_heap;
}

void PathFinder::init(std::shared_ptr<const DGraph> g) {
    m_graph = g;
}

void PathFinder::init_arrays (
        std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        bool *m_open_vec,
        bool *m_closed_vec,
        const unsigned int v,
        const size_t n)
{
    w [v] = 0.0;
    d [v] = 0.0;
    prev [v] = -1;
    m_open_vec [v] = true;

    std::fill (m_open_vec, m_open_vec + n, false);
    std::fill (m_closed_vec, m_closed_vec + n, false);
}

void PathFinder::relax (
        const DGraphEdge *edge,
        std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        bool *m_open_vec,
        const unsigned int &v0,
        const unsigned int &target)
{
    double wt = w [v0] + edge->wt;
    if (wt < w [target]) {
        d [target] = d [v0] + edge->dist;
        w [target] = wt;
        prev [target] = static_cast <int> (v0);

        if (m_open_vec [target]) {
            m_heap->decreaseKey(target, wt);
        }
        else {
            m_heap->insert (target, wt);
            m_open_vec [target] = true;
        }
    }
}

void PathFinder::relax_heur (const DGraphEdge *edge,
        std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        bool *m_open_vec,
        const unsigned int &v0,
        const unsigned int &target,
        const std::vector<double> &heur,    // heuristic for A*
        const double &dmax,                 // used for reverse bidirectional
        const bool &reverse)                // reverse dir of bidirectional 
{
    double wt = w [v0] + edge->wt;
    if (wt < w [target]) {
        d [target] = d [v0] + edge->dist;
        w [target] = wt;
        prev [target] = static_cast <int> (v0);

        if (!reverse)
        {
            if (m_open_vec [target]) {
                m_heap->decreaseKey(target, wt + heur [target] - heur [v0]);
            }
            else {
                m_heap->insert (target, wt + heur [target] - heur [v0]);
                m_open_vec [target] = true;
            }
        } else
        {
            if (m_open2 [target]) {
                m_heap_rev->decreaseKey (target,
                        wt + dmax - (heur [target] - heur [v0]));
            }
            else {
                m_heap_rev->insert (target,
                        wt + dmax - (heur [target] - heur [v0]));
                m_open2 [target] = true;
            }
        }
    }
}

/**********************************************************************
 ************************  PATH ALGORITHMS    *************************
 **********************************************************************/

void PathFinder::Dijkstra (
        std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        unsigned int v0)
{
    const DGraphEdge *edge;

    const unsigned int n = m_graph->nVertices();
    const std::vector<DGraphVertex>& vertices = m_graph->vertices();

    PathFinder::init_arrays (d, w, prev, m_open, m_closed, v0, n);
    m_heap->insert(v0, 0.0);

    while (m_heap->nItems() > 0) {
        unsigned int v = m_heap->deleteMin();

        m_closed [v] = true;
        m_open [v] = false;

        edge = vertices [v].outHead;
        while (edge) {
            unsigned int et = edge->target;

            if (!m_closed [et]) {
                PathFinder::relax (edge, d, w, prev, m_open, v, et);
            }

            edge = edge->nextOut;
        } // end while edge
    } // end while nItems > 0
}

void PathFinder::AStar (std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        const std::vector<double>& heur,
        unsigned int v0)
{
    const DGraphEdge *edge;

    const unsigned int n = m_graph->nVertices();
    const std::vector<DGraphVertex>& vertices = m_graph->vertices();

    PathFinder::init_arrays (d, w, prev, m_open, m_closed, v0, n);
    m_heap->insert(v0, heur [v0]);

    while (m_heap->nItems() > 0) {
        unsigned int v = m_heap->deleteMin();

        m_closed [v] = true;
        m_open [v] = false;

        edge = vertices [v].outHead;
        while (edge) {
            unsigned int et = edge->target;

            if (!m_closed [et]) {
                PathFinder::relax_heur (edge, d, w, prev, m_open, v, et, heur,
                        0.0, false);
            }

            edge = edge->nextOut;
        } // end while edge
    } // end while m_heap->nItems
}

// bi-directional A*
void PathFinder::AStar2 (std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        const std::vector<double>& heur,
        unsigned int v0, unsigned int v1)
{
    const DGraphEdge *edge;

    const unsigned int n = m_graph->nVertices();
    const std::vector<DGraphVertex>& vertices = m_graph->vertices();

    PathFinder::init_arrays (d, w, prev, m_open, m_closed, v0, n);
    std::vector <double> d_rev (n, INFINITE_DOUBLE), w_rev (n, INFINITE_DOUBLE);
    std::vector <int> prev_rev (n, INFINITE_INT);
    PathFinder::init_arrays (d_rev, w_rev, prev_rev, m_open2, m_closed2, v1, n);

    // m_heap holds heuristic estimates from source v0 to target vertex v1
    // m_heap_rev holds reverse: from target v1 to source v0
    m_heap->insert(v0, heur [v1]);
    double dmax = heur [v1];
    m_heap_rev->insert (v1, dmax - heur [v0]); // = heur [v1]

    std::unordered_set <unsigned int> frontier, backward;

    while (m_heap->nItems() > 0 && m_heap_rev->nItems() > 0) {
        //if (m_heap->getmin () <= m_heap_rev->getmin ())
        if (m_heap->nItems () >= m_heap_rev->nItems ())
        {
            unsigned int v = m_heap->deleteMin();
            m_open [v] = false;
            m_closed [v] = true;

            if (m_closed2 [v])
            {
                frontier.emplace (v);
            } else
            {
                edge = vertices [v].outHead;
                while (edge) {
                    unsigned int et = edge->target;

                    if (!m_closed [et]) {
                        PathFinder::relax_heur (edge, d, w, prev, m_open, v, et, heur,
                                0.0, false);
                    }

                    edge = edge->nextOut;
                } // end while edge
            } // end else !m_closed2
        } else // else m_heap_rev.nItems > m_heap.nItems
        {
            unsigned int v = m_heap_rev->deleteMin();
            m_open2 [v] = false;
            m_closed2 [v] = true;

            backward.emplace (v);

            if (m_closed [v])
            {
                //frontier.emplace (v);
            } else
            {
                edge = vertices [v].inHead;
                while (edge) {
                    unsigned int et = edge->source;

                    if (!m_closed2 [et]) {
                        PathFinder::relax_heur (edge, d_rev, w_rev, prev_rev,
                                m_open2, v, et, heur, dmax, true);
                    }

                    edge = edge->nextIn;
                } // end while edge
            } // end else !m_closed [v]
        } // end else m_heap_rev.nItems > m_heap.nItems
    } // end while m_heap->nItems

    /*
    int count1 = 0, count2 = 0;
    for (int i = 0; i < n; i++)
    {
        if (m_open [i])
        {
            count1++;
        }
        if (m_open2 [i])
        {
            count2++;
        }
    }
    Rcpp::Rcout << "(f, b) have (" << count1 << ", " << count2 <<
        ") nodes still open" << std::endl;

    // Reconstruct all distances from the frontier
    Rcpp::Rcout << "(frontier, backward) = (" << frontier.size () <<
        ", " << backward.size () << ") / " << n << std::endl;
    */

    // TODO: This is wrong
    for (auto fr:frontier)
        for (auto b:backward)
        {
            double wtemp = w [fr] + w_rev [b];
            if (wtemp < w [b])
            {
                w [b] = wtemp;
                d [b] = d [fr] + d_rev [b];
            }
        }
}

// Only sightly modified from the above, to use EdgeSet edge_set instead of
// m_heap.
void PathFinder::Dijkstra_set (std::vector<double>& d,
        std::vector<double>& w,
        std::vector<int>& prev,
        unsigned int v0)
{
    const DGraphEdge *edge;

    const unsigned int n = m_graph->nVertices();
    const std::vector<DGraphVertex>& vertices = m_graph->vertices();

    std::fill (m_closed, m_closed + n, false);
    std::fill (m_open, m_open + n, false);

    w [v0] = 0.0;
    d [v0] = 0.0;
    prev [v0] = -1;
    m_heap->insert(v0, 0.0);
    m_open [v0] = true;

    edge_set.insert (DijkstraEdge (0.0, v0));

    while (edge_set.size () > 0) {
        EdgeSet::iterator ei = edge_set.begin();
        unsigned int v = ei->geti();
        edge_set.erase (ei);

        m_closed [v] = true;
        m_open [v] = false;

        edge = vertices [v].outHead;
        while (edge) {
            unsigned int et = edge->target;

            if (!m_closed [et]) {
                double wt = w [v] + edge->wt;
                if (wt < w [et]) {
                    d [et] = d [v] + edge->dist;
                    double wold = w [et];
                    w [et] = wt;
                    prev [et] = static_cast <int> (v);
                    if (m_open [et])
                    {
                        DijkstraEdge de (wold, et);
                        if (edge_set.find (de) != edge_set.end ())
                            edge_set.erase (edge_set.find (de));
                    } else
                        m_open [et] = true;
                    edge_set.insert (DijkstraEdge (w [et], et));
                }
            }

            edge = edge->nextOut;
        } // end while edge
    } // end while edge_set.size
}