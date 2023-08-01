
#include "TPRTreeMemory.h"
#include <spatialindex/SpatialIndex.h>
#include "stopwatch.h"
#include <limits>

using namespace SpatialIndex;
using namespace std;

#define INSERT 1
#define DELETE 0
#define QUERY 2

class MyVisitor : public IVisitor {
public:
    void visitNode(const INode &) override {}

    void visitData(const IData &d) override {
//        cout << d.getIdentifier() << endl;
        // the ID of this data entry is an answer to the query. I will just print it to stdout.
    }

    void visitData(std::vector<const IData *> &) override {}
};

int main(int argc, char **argv) {
    ifstream fin(argv[1]);
    if (!fin) {
        cerr << "Cannot open data file " << argv[1] << "." << endl;
        return -1;
    }

    IStorageManager *ism = StorageManager::createNewMemoryStorageManager();
    StorageManager::IBuffer *file = StorageManager::createNewRandomEvictionsBuffer(*ism, 10, false);
    id_type indexIdentifier;
    ISpatialIndex *tree = TPRTree::createNewTPRTree(*file, 0.7, atoi(argv[2]), atoi(argv[2]), 2,
                                                    SpatialIndex::TPRTree::TPRV_RSTAR, 20, indexIdentifier);

    size_t count = 0;
    id_type id;
    size_t op;
    double ax, vx, ay, vy, ct, rt, unused;
    double plow[2], phigh[2]; // boundary of moving region
    double pvlow[2], pvhigh[2];
    Stopwatch sw;

    size_t n_ins = 0, n_del = 0, n_qry = 0;
    double t_ins = 0, t_del = 0, t_qry = 0;

    while (fin) {
        fin >> id >> op >> ct >> rt >> unused >> ax >> vx >> unused >> ay >> vy;
        if (!fin.good()) continue; // skip newlines, etc.

        if (op == INSERT) {
            sw.start();
            plow[0] = ax;
            plow[1] = ay;
            phigh[0] = ax;
            phigh[1] = ay;
            pvlow[0] = vx;
            pvlow[1] = vy;
            pvhigh[0] = vx;
            pvhigh[1] = vy;
            Tools::Interval ivT(ct, std::numeric_limits<double>::max());

            MovingRegion r = MovingRegion(plow, phigh, pvlow, pvhigh, ivT, 2);

            //ostringstream os;
            //os << r;
            //string data = os.str();
            // associate some data with this region. I will use a string that represents the
            // region itself, as an example.
            // NOTE: It is not necessary to associate any data here. A null pointer can be used. In that
            // case you should store the data externally. The index will provide the data IDs of
            // the answers to any query, which can be used to access the actual data from the external
            // storage (e.g. a hash table or a database table, etc.).
            // Storing the data in the index is convinient and in case a clustered storage manager is
            // provided (one that stores any node in consecutive pages) performance will improve substantially,
            // since disk accesses will be mostly sequential. On the other hand, the index will need to
            // manipulate the data, resulting in larger overhead. If you use a main memory storage manager,
            // storing the data externally is highly recommended (clustering has no effect).
            // A clustered storage manager is NOT provided yet.
            // Also you will have to take care of converting you data to and from binary format, since only
            // array of bytes can be inserted in the index (see RTree::Node::load and RTree::Node::store for
            // an example of how to do that).

            //tree->insertData(data.size() + 1, reinterpret_cast<const uint8_t*>(data.c_str()), r, id);

            tree->insertData(0, nullptr, r, id);
            sw.stop();
            n_ins++;
            t_ins += sw.ms();
            // example of passing zero size and a null pointer as the associated data.
        } else if (op == DELETE) {
            sw.start();
            plow[0] = ax;
            plow[1] = ay;
            phigh[0] = ax;
            phigh[1] = ay;
            pvlow[0] = vx;
            pvlow[1] = vy;
            pvhigh[0] = vx;
            pvhigh[1] = vy;
            Tools::Interval ivT(rt, ct);

            MovingRegion r = MovingRegion(plow, phigh, pvlow, pvhigh, ivT, 2);

            if (tree->deleteData(r, id) == false) {
                cerr << "******ERROR******" << endl;
                cerr << "Cannot delete id: " << id << " , count: " << count << endl;
                return -1;
            }
            sw.stop();
            n_del++;
            t_del += sw.ms();
        } else if (op == QUERY) {
            sw.start();
            plow[0] = ax;
            plow[1] = ay;
            phigh[0] = vx;
            phigh[1] = vy;
            pvlow[0] = 0.0;
            pvlow[1] = 0.0;
            pvhigh[0] = 0.0;
            pvhigh[1] = 0.0;

            Tools::Interval ivT(ct, rt);

            MovingRegion r = MovingRegion(plow, phigh, pvlow, pvhigh, ivT, 2);
            MyVisitor vis;

            tree->intersectsWithQuery(r, vis);
            // this will find all data that intersect with the query range.
            sw.stop();
            n_qry++;
            t_qry += sw.ms();
        }

        if ((count % 1000) == 0)
            cerr << count << endl;

        count++;
    }

    cerr << "Operations: " << count << endl;
    cerr << *tree;
    cerr << "Buffer hits: " << file->getHits() << endl;
    cerr << "Index ID: " << indexIdentifier << endl;

    cerr << "Inserts: " << n_ins << " Time: " << t_ins << " Avg: " << t_ins / n_ins * 1000 << " us" << endl;
    cerr << "Deletes: " << n_del << " Time: " << t_del << " Avg: " << t_del / n_del * 1000 << " us" << endl;
    cerr << "Queries: " << n_qry << " Time: " << t_qry << " Avg: " << t_qry / n_qry * 1000 << " us" << endl;

    bool ret = tree->isIndexValid();
    if (ret == false) cerr << "ERROR: Structure is invalid!" << endl;
    else cerr << "The stucture seems O.K." << endl;

    delete tree;
    delete file;
}