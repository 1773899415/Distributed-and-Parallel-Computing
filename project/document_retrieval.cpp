#include <iostream>
#include <fstream>
#include <sstream>

#include <string>
#include <vector>
#include <algorithm>

#include <mutex>
#include <thread>

#include <ctime>

using namespace std;

const int S = 256;

bool judgeCycle(const string& p) {
    int lenp = p.length(), half = lenp / 2;
    for (int i = lenp - 1; i > half; --i)
        if (p.substr(0, i) == p.substr(lenp - i))
            return true;
    return false;
}

void kmpInit(vector<int>& ne, const string& p) {
    int lenp = p.length();
    ne[0] = -1;
    for (int i = 1, j = -1; i < lenp; ++i) {
        while (j >= 0 && p[i] != p[j + 1])
            j = ne[j];
        if (p[i] == p[j + 1])
            ++j;
        ne[i] = j;
    }
}

void kmpMatch(vector<int>& ans, const vector<int>& ne, const string& t, const string& p, int start, int end, mutex& ansMutex) {
    int lent = t.length(), lenp = p.length();
    for (int i = start, j = -1; i < end; ++i) {
        while (j != -1 && t[i] != p[j + 1])
            j = ne[j];
        if (t[i] == p[j + 1])
            ++j;
        if (j == lenp - 1) {
            {
                lock_guard<mutex> lock(ansMutex);
                ans.push_back(i - j);
            }
            j = ne[j];
        }
    }
}

void bmInit(vector<int>& badt, vector<int>& goodt, const string& p) {
    int lenp = p.length();
    for (int i = 0; i < lenp - 1; ++i)
        badt[p[i]] = lenp - 1 - i;
    goodt[lenp - 1] = 1;
    int last_prelen = 0;
    for (int i = lenp - 1; i > 0; --i) {
        int prelen = 0;
        int suflen = lenp - i;
        if (p.substr(0, lenp - i) == p.substr(i)) {
            prelen = suflen;
            last_prelen = prelen;
        }
        else
            prelen = last_prelen;
        goodt[i - 1] = lenp + suflen - prelen;
    }
    for (int i = 0; i < lenp - 1; ++i) {
        string psuffix = p.substr(0, i + 1);
        int j;
        for (j = 0; j < i + 1; ++j)
            if (psuffix[i - j] != p[lenp - 1 - j])
                break;
        if (j > 0) {
            int t = i + 1 - j;
            int k = lenp - 1 - j;
            goodt[k] = lenp - t;
        }
    }
}

void bmMatch(vector<int>& ans, const vector<int>& badt, const vector<int>& goodt, const string& t, const string& p, int start, int end, mutex& ansMutex) {
    int lent = t.length(), lenp = p.length();
    int i = start + lenp - 1;
    while (i < end) {
        int j = lenp - 1;
        while (j >= 0 && p[j] == t[i])
            --j, --i;
        if (j < 0) {
            {
                lock_guard<mutex> lock(ansMutex);
                ans.push_back(i + 1);
            }
            i += lenp + 1;
        }
        else
            i += max(badt[t[i]], goodt[j]);
    }
}

void chooseMethod(bool flag, vector<int>& ans, const vector<int>& ne, const vector<int>& badt, const vector<int>& goodt, const string& t, const string& p, int start, int end, mutex& ansMutex) {
    if (flag)
        kmpMatch(ans, ne, t, p, start, end, ansMutex);
    else
        bmMatch(ans, badt, goodt, t, p, start, end, ansMutex);
}

void read_chunk(const string& path_document, int start, int end, size_t index, vector<vector<char>>& result) {
    ifstream infile(path_document, ios::binary);
    infile.seekg(start, ios::beg);
    int block_size = end - start;
    vector<char> tmpv(block_size);
    infile.read(tmpv.data(), block_size);
    int i = 0;
    for (char c : tmpv)
        if (c != 13)
            result[index][i++] = c;
    result[index].resize(i);
}

int main() {

    clock_t start = clock();

    int num_threads = thread::hardware_concurrency();
    if (num_threads == 0)
        num_threads = 4;

    const string path_document = "./data/document_retrieval/document.txt";
    ifstream infile1(path_document);
    infile1.seekg(0, ios::end);
    int file_size = infile1.tellg();
    int block_size = file_size / num_threads;
    infile1.close();
    vector<vector<char>> result(num_threads);
    vector<thread> threadsRead;
    for (int i = 0; i < num_threads; ++i) {
        int start = i * block_size;
        int end = (i == num_threads - 1) ? file_size : start + block_size;
        result[i].resize(end - start);
        threadsRead.emplace_back(read_chunk, cref(path_document), start, end, i, ref(result));
    }
    for (thread& th : threadsRead)
        th.join();
    string t = "";
    for (size_t i = 0; i < num_threads; ++i)
        t += string(result[i].begin(), result[i].end());
    int lent = t.length();
    int lenSegment = lent / num_threads;

    const string path_target = "./data/document_retrieval/target.txt";
    ifstream infile2(path_target);
    string p;

    while (getline(infile2, p)) {
        int lenp = p.length();
        bool flag = false;
        vector<thread> threads;
        vector<int> ans;
        vector<int> ne(lenp, 0);
        vector<int> badt(S, lenp);
        vector<int> goodt(lenp, 1);

        if (judgeCycle(p)) {
            flag = true;
            kmpInit(ne, p);
        }
        else {
            flag = false;
            bmInit(badt, goodt, p);
        }

        mutex ansMutex;
        for (int i = 0; i < num_threads; ++i) {
            int start = i * lenSegment;
            int end = (i == num_threads - 1) ? lent : start + lenSegment + lenp - 1;
            end = min(end, lent);
            threads.emplace_back(chooseMethod, flag, ref(ans), cref(ne), cref(badt), cref(goodt), cref(t), cref(p), start, end, ref(ansMutex));
        }

        for (thread& th : threads)
            th.join();

        sort(ans.begin(), ans.end());

        cout << ans.size() << " ";
        for (int& a : ans)
            cout << a << " ";
        cout << endl;

        //ofstream outfile("./data/document_retrieval/result_document.txt", ios::out | ios::app);
        //outfile << ans.size() << " ";
        //for (auto& a : ans)
        //    outfile << a << " ";
        //outfile << endl;
    }
    infile2.close();

    clock_t end = clock();
    cout << "»¨·ÑÁË" << (double)(end - start) / CLOCKS_PER_SEC << "Ãë" << endl;

    return 0;
}
