#include "utils.hpp"
#include "index_types.hpp"
#include "collection.hpp"

#include "logging.hpp"
#include "timings.hpp"

using namespace std::chrono;

typedef struct cmdargs {
    std::string collection_dir;
    std::string pattern_file;
} cmdargs_t;

void print_usage(const char* program)
{
    fprintf(stdout, "%s -c <collection dir> -p <pattern file>\n", program);
    fprintf(stdout, "where\n");
    fprintf(stdout, "  -c <collection dir>  : the collection dir.\n");
    fprintf(stdout, "  -p <pattern file>    : the pattern file.\n");
};

cmdargs_t parse_args(int argc, const char* argv[])
{
    cmdargs_t args;
    int op;
    args.collection_dir = "";
    while ((op = getopt(argc, (char* const*)argv, "c:p:")) != -1) {
        switch (op) {
            case 'c':
                args.collection_dir = optarg;
                break;
            case 'p':
                args.pattern_file = optarg;
                break;
        }
    }
    if (args.collection_dir == ""||args.pattern_file == "") {
        LOG(FATAL) << "Missing command line parameters.";
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    return args;
}

template <class t_idx>
void bench_index(collection& col,const std::vector<gapped_pattern>& patterns)
{
    /* load index */
    t_idx idx(col);
    LOG(INFO) << "BENCH_INDEX (" << idx.name() << ")";

    /* benchmark */
    size_t num_results = 0;
    size_t checksum = 0;
    size_t npat = 1;
    timing_results t_prep;
    timing_results t;
    string total_info = "";
    for (const auto& pat : patterns) {
        // give index a chance to output relevant 
        // information about the upcoming query
        total_info += "," + idx.info(pat);
        
        // let index perform text-INDEPENDENT work
        gm_timer tm_prep("PAT_PREP");
        idx.prepare(pat);
        auto dur_prep = tm_prep.elapsed();
        t_prep.add_timing(dur_prep);
        
        // perform search
        gm_timer tm("PAT_SEARCH");
        auto res = idx.search(pat);
        auto dur = tm.elapsed();
        t.add_timing(dur);

        /* compute checksum */
        for (const auto& pos : res.positions) {
            checksum += pos;
            // std::cerr << pos << std::endl;
            num_results++;
        }

        auto time_mus_prep = duration_cast<microseconds>(dur_prep);
        auto time_mus = duration_cast<microseconds>(dur);
        LOG(INFO) << " NPAT=" << npat++ << " NPOS=" << res.positions.size()
                  << " TIME_MS_PREP=" << time_mus_prep.count()
                  << " TIME_MS=" << time_mus.count() << "  P='"<<pat.raw_regexp<<"'";
    }

    /* output stats */
    auto ts = t.summary();
    auto ts_prep = t_prep.summary();
    
    LOG(INFO) << "SUMMARY";
    LOG(INFO) << " num_patterns = " << t.timings.size();
    LOG(INFO) << " checksum = " << checksum;
    
    LOG(INFO) << " total_time_mus = " << duration_cast<microseconds>(ts.total).count();
    LOG(INFO) << " min_time_mus = " << duration_cast<microseconds>(ts.min).count();
    LOG(INFO) << " qrt_1st_time_mus = " << duration_cast<microseconds>(ts.qrt_1st).count();
    LOG(INFO) << " mean_time_mus = " << duration_cast<microseconds>(ts.mean).count();
    LOG(INFO) << " median_time_mus = " << duration_cast<microseconds>(ts.median).count();
    LOG(INFO) << " qrt_3rd_time_mus = " << duration_cast<microseconds>(ts.qrt_3rd).count();
    LOG(INFO) << " max_time_mus = " << duration_cast<microseconds>(ts.max).count();

    std::cout << "# info =" << total_info << std::endl;

    std::cout << "# num_results = " << num_results << std::endl;
    std::cout << "# checksum = " << checksum << std::endl;
    std::cout << "# total_time_mus = " << duration_cast<microseconds>(ts.total).count() << std::endl;
    std::cout << "# min_time_mus = " << duration_cast<microseconds>(ts.min).count() << std::endl;
    std::cout << "# qrt_1st_time_mus = " << duration_cast<microseconds>(ts.qrt_1st).count() << std::endl;
    std::cout << "# mean_time_mus = " << duration_cast<microseconds>(ts.mean).count() << std::endl;
    std::cout << "# median_time_mus = " << duration_cast<microseconds>(ts.median).count() << std::endl;
    std::cout << "# qrt_3rd_time_mus = " << duration_cast<microseconds>(ts.qrt_3rd).count() << std::endl;
    std::cout << "# max_time_mus = " << duration_cast<microseconds>(ts.max).count() << std::endl;
    
    std::cout << "# prep_total_time_mus = " << duration_cast<microseconds>(ts_prep.total).count() << std::endl;
    std::cout << "# prep_min_time_mus = " << duration_cast<microseconds>(ts_prep.min).count() << std::endl;
    std::cout << "# prep_qrt_1st_time_mus = " << duration_cast<microseconds>(ts_prep.qrt_1st).count() << std::endl;
    std::cout << "# prep_mean_time_mus = " << duration_cast<microseconds>(ts_prep.mean).count() << std::endl;
    std::cout << "# prep_median_time_mus = " << duration_cast<microseconds>(ts_prep.median).count() << std::endl;
    std::cout << "# prep_qrt_3rd_time_mus = " << duration_cast<microseconds>(ts_prep.qrt_3rd).count() << std::endl;
    std::cout << "# prep_max_time_mus = " << duration_cast<microseconds>(ts_prep.max).count() << std::endl;
}

int main(int argc, const char* argv[])
{
    log::start_log(argc, argv, false);

    /* parse command line */
    cmdargs_t args = parse_args(argc, argv);

    /* parse collection directory */
    collection col(args.collection_dir);

    /* parse pattern file */
    auto patterns = utils::parse_pattern_file(args.pattern_file);

    /* create index */
    {
        using index_type = INDEX_TYPE;
        bench_index<index_type>(col,patterns);
    }

    return 0;
}
