#include <sdsl/suffix_arrays.hpp>
#include <vector>
#include <iostream>
#include "../include/sdsl/matching.hpp"

using namespace sdsl;
using namespace std;

using namespace std::chrono;
using timer = std::chrono::high_resolution_clock;
 

template <typename type_csa, typename type_wt>
size_t match_ref(type_csa csa, type_wt wts, string s1, string s2, size_t min_gap, size_t max_gap, std::function<void(typename type_csa::size_type a, typename type_csa::size_type b)> callback)
{
    using size_type = typename type_csa::size_type;

    size_type sp1, ep1;
    auto cnt1 = backward_search(csa, 0, csa.size()-1, s1.begin(), s1.end(), sp1, ep1);

    size_type sp2, ep2;
    auto cnt2 = backward_search(csa, 0, csa.size()-1, s2.begin(), s2.end(), sp2, ep2);

    vector<size_type> p2;

    if (cnt1 == 0 || cnt2 == 0)
        return 0;

    for (auto i2 = sp2; i2 <= ep2; i2++)
        p2.push_back(wts[i2]);

    sort(p2.begin(), p2.end());

    size_t result = 0;
    for (auto i1 = sp1; i1 <= ep1; i1++)
    {
        size_type l1 = wts[i1];
        auto i2a = lower_bound(p2.begin(), p2.end(), l1 + min_gap);
        auto i2b = upper_bound(p2.begin(), p2.end(), l1 + max_gap);

        for (auto i2 = i2a; i2 < i2b; i2++)
        {
            callback(l1, *i2);
            result++;
        } 
    }
    return result;
}
 
template <typename type_matching_index>
size_t match_dfs(type_matching_index& index, string s1, string s2, size_t min_gap, size_t max_gap, std::function<void(typename type_matching_index::size_type a, typename type_matching_index::size_type b)> callback)
{
    size_t cnt = 0;
    for (auto res : index.match2(s1, incremental_wildcard_pattern(s2, min_gap, max_gap)))
    {
        callback(res.first, res.second);
        ++cnt;
    }
    return cnt;
}

template <typename type_matching_index>
size_t match_dfs_ex(type_matching_index& index, 
    string s, 
    incremental_wildcard_pattern p1)
{
    size_t cnt = 0;
    for (auto res : index.match2(s, p1))
    {
        cout << res.first << " " << res.second << endl;
        ++cnt;
    }
    return cnt;
}

int main(int argc, char* argv[])
{
    if ( argc < 2 ){
        cout << "Usage: ./" << argv[0] << " file_to_index" << endl;
        cout << "    Input pat1 pat2 min_gap max_gap" << endl;
        return 1;
    }
    csa_wt<wt_huff<rrr_vector<63>>> csa;

    cache_config cc(false,".","WILD_CARD_MATCH_TMP");
    construct(csa, argv[1], cc, 1);

    wt_int<bit_vector, rank_support_v5<>, select_support_scan<1>, select_support_scan<0>> wts;
    
    construct(wts, cache_file_name(conf::KEY_SA, cc));

    util::delete_all_files(cc.file_map);

    matching_index<> index(csa, wts);

    cout<<"wts.size()="<<wts.size()<<endl;
    if ( wts.size() < 100 ){
        cout<<"wts="<<wts<<endl;
    }
    
    // CALLBACKS
    using size_type = decltype(csa)::size_type;
    std::function<void(size_type a, size_type b)> callback_cout = [&](size_type a, size_type b) 
    { 
        if (b - a > 100)
            cout << "\t" << a << " " << b << endl; 
        else
            cout << "\t" << extract(csa, a, b) << endl; 
    };
    std::function<void(size_type a, size_type b)> callback_dbg = [&](size_type a, size_type b) 
    {
        cout << "\t" << a << " " << b << endl; 
    };
    std::function<void(size_type a, size_type b)> callback_nop = [](size_type a, size_type b) { (void)a; (void)b; };

    auto compare = [&](string s1, string s2, size_t min_gap, size_t max_gap) 
    { 
        cout << "Comparing with: s1=" << s1 << ", s2=" << s2 << ", min_gap=" << min_gap << ", max_gap=" << max_gap << endl;
        
        size_t matches = match_dfs(index, s1, s2, min_gap, max_gap, callback_cout);
        size_t matches_ref = match_ref(csa, wts, s1, s2, min_gap, max_gap, callback_nop);
            
        bool success = matches == matches_ref;
        if (!success)
        {
            cout << "Error" << endl;
            cout << "MATCHES:     " << matches << endl;
            cout << "MATCHES_ref: " << matches_ref << endl;
        }
        else
            cout << matches << " found" << endl;
        
        return success;
    };
    
    bool test_and_bench = false;
    if (test_and_bench)
    {
        // TESTS
        vector<pair<string, string>> tcs;
        tcs.emplace_back("8", "8");
        tcs.emplace_back("8", "0");
        tcs.emplace_back("a", "1");
        tcs.emplace_back(" ", "8");
        int max = 100;
        for (unsigned int tci = 0; tci < tcs.size(); tci++)
        {
            auto s1 = tcs[tci].first;
            auto s2 = tcs[tci].second;
            cout << "TEST CASE: " << s1 << " " << s2 << endl;
            for (int i = 0; i <= max; i += 3)
            { 
                if (!compare(s1, s2, i, max)) goto err;
                if (!compare(s1, s2, 0, i)) goto err;
                if (!compare(s1, s2, i, i)) goto err;
                if (!compare(s1, s2, i, 2*i)) goto err;
                if (!compare(s1, s2, 10 * i, 10 * i + 3)) goto err;
                if (!compare(s1, s2, 10 * i, 10 * i + 10)) goto err;
            }
        }
         
        // BENCH
        {
            size_t found = 0;
            auto start = timer::now();
            for (unsigned int tci = 0; tci < tcs.size(); tci++)
            {
                auto s1 = tcs[tci].first;
                auto s2 = tcs[tci].second;
                for (int i = 0; i <= max; i += 3)
                { 
                    found += match_dfs(index, s1, s2, i, max, callback_nop);
                    found += match_dfs(index, s1, s2, 0, i, callback_nop);
                    found += match_dfs(index, s1, s2, i, i, callback_nop);
                    found += match_dfs(index, s1, s2, i, 2*i, callback_nop);
                    found += match_dfs(index, s1, s2, 10 * i, 10 * i + 3, callback_nop);
                    found += match_dfs(index, s1, s2, 10 * i, 10 * i + 10, callback_nop);
                }
            }
            auto stop = timer::now();
            cout << "BENCH_DFS: " << duration_cast<milliseconds>(stop-start).count() << "ms for " << found << " occurrences" << endl;
        
            found = 0;
            start = timer::now();
            for (unsigned int tci = 0; tci < tcs.size(); tci++)
            {
                auto s1 = tcs[tci].first;
                auto s2 = tcs[tci].second;
                for (int i = 0; i <= max; i += 3)
                { 
                    found += match_ref(csa, wts, s1, s2, i, max, callback_nop);
                    found += match_ref(csa, wts, s1, s2, 0, i, callback_nop);
                    found += match_ref(csa, wts, s1, s2, i, i, callback_nop);
                    found += match_ref(csa, wts, s1, s2, i, 2*i, callback_nop);
                    found += match_ref(csa, wts, s1, s2, 10 * i, 10 * i + 3, callback_nop);
                    found += match_ref(csa, wts, s1, s2, 10 * i, 10 * i + 10, callback_nop);
                }
            }
            stop = timer::now();
            cout << "BENCH_REF: " << duration_cast<milliseconds>(stop-start).count() << "ms for " << found << " occurrences" << endl;
        }
    }

    // PROMPT
    { 
        size_t min1_gap;
        size_t max1_gap;
        //size_t min2_gap;
        //size_t max2_gap;
        string s1, s2, s3;
        cout << "PROMPT: " << endl;
        while (cin >> s1 >> s2 >> min1_gap >> max1_gap)
            compare(s1, s2, min1_gap, max1_gap);
            //cout << match_dfs(index, s1, s2, min1_gap, max1_gap, callback_cout) << " matches found" << endl;
        //while (cin >> s1 >> min1_gap >> max1_gap >> s2 >> min2_gap >> max2_gap >> s3)
        //    cout << match3_dfs(index, 
        //        s1,
        //        incremental_wildcard_pattern(s2, min1_gap + s1.size(), max1_gap + s1.size()),
        //        incremental_wildcard_pattern(s3, min2_gap + s2.size(), max2_gap + s2.size())
        //        ) << " matches found" << endl;
        //while (cin >> s1 >> s2 >> min1_gap >> max1_gap >> s3)
        //    cout << match_dfs_ex(index, 
        //        s1,
        //        incremental_wildcard_pattern(s2, min1_gap + s1.size(), max1_gap + s1.size(), s3)
        //        ) << " matches found" << endl;
    }
    err:;
}

