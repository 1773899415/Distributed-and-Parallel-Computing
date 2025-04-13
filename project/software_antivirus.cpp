#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <vector>
#include <string>
#include <queue>
#include <functional>
#include <algorithm>

#include <mutex>
#include <thread>
#include <condition_variable>

#include <ctime>

using namespace std;
namespace fs = std::filesystem;

const int NVIRUS = 10;

const int NNODE = 1e6;

int idx = 0;

void insertTrie(vector<vector<int>>& tr, vector<int>& flag, const string& p, int i) {
    int f = 0;
    for (char c : p) {
        for (int i = 7; i >= 0; --i) {
            int s = c >> i & 0x01;
            if (!tr[f][s])
                tr[f][s] = ++idx;
            f = tr[f][s];
        }
    }
    flag[f] = i;
}

void buildTrie(vector<vector<int>>& tr, vector<int>& ne) {
    vector<int> q(NNODE);
    int hh = 0, tt = -1;
    for (int i = 0; i < 2; ++i)
        if (tr[0][i])
            q[++tt] = tr[0][i];
    while (hh <= tt) {
        int c = q[hh++];
        for (int i = 0; i < 2; ++i) {
            if (!tr[c][i])
                tr[c][i] = tr[ne[c]][i];
            else {
                ne[tr[c][i]] = tr[ne[c]][i];
                q[++tt] = tr[c][i];
            }
        }
    }
}

class ThreadPool {
private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex queueMutex;
    condition_variable condition;
    bool stop;
public:
    ThreadPool(int numThreads);
    ~ThreadPool();
    void enqueue(function<void()> task);
};

ThreadPool::ThreadPool(int numThreads) : stop(false) {
    for (int i = 0; i < numThreads; ++i) {
        workers.emplace_back([this]() {
            while (true) {
                function<void()> task;
                {
                    unique_lock<mutex> lock(this->queueMutex);
                    this->condition.wait(lock,
                        [this]() { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        unique_lock<mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (thread& worker : workers)
        worker.join();
}

void ThreadPool::enqueue(function<void()> task) {
    {
        unique_lock<mutex> lock(queueMutex);
        tasks.emplace(task);
    }
    condition.notify_one();
}

void readMatch(const string& path, const vector<vector<int>>& tr, const vector<int>& flag, mutex& coutMutex) {
    ifstream infile(path, ios::binary);
    vector<int> ans;
    stringstream tmp;
    tmp << infile.rdbuf();
    string t = tmp.str();
    int f = 0;
    for (char c : t) {
        for (int i = 7; i >= 0; --i) {
            int s = c >> i & 0x01;
            f = tr[f][s];
            if (flag[f] != 0)
                ans.push_back(flag[f]);
        }
    }
    if (ans.size() > 0) {
        sort(ans.begin(), ans.end());
        {
            lock_guard<mutex> lock(coutMutex);
            cout << path.substr(2) << " ";
            for (int a : ans) {
                string index = to_string(a);
                index = string(2 - index.length(), '0') + index;
                cout << "virus" + index + ".bin" << " ";
            }
            cout << endl;
        }
        //{
        //    lock_guard<mutex> lock(coutMutex);
        //    ofstream outfile("./data/software_antivirus/result_software.txt", ios::out | ios::app);
        //    outfile << path.substr(2) << " ";
        //    for (int a : ans) {
        //        string index = to_string(a);
        //        index = string(2 - index.length(), '0') + index;
        //        outfile << "virus" + index + ".bin" << " ";
        //    }
        //    outfile << endl;
        //}
    }
    infile.close();
}

int main() {

    clock_t start = clock();

    vector<vector<int>> tr(NNODE, vector<int>(2, 0));
    vector<int> flag(NNODE, 0);
    vector<int> ne(NNODE, 0);

    for (int i = 1; i <= NVIRUS; ++i) {
        string index = to_string(i);
        index = string(2 - index.length(), '0') + index;
        string path = "./data/software_antivirus/virus/virus" + index + ".bin";
        ifstream infile(path, ios::binary);
        ostringstream tmp;
        tmp << infile.rdbuf();
        string p = tmp.str();
        insertTrie(tr, flag, p, i);
        infile.close();
    }

    buildTrie(tr, ne);

    int numThreads = thread::hardware_concurrency();
    if (numThreads == 0) 
        numThreads = 4;

    ThreadPool pool(numThreads);
    mutex coutMutex;
    string path_directory = "./data/software_antivirus/opencv-4.10.0";
    for (auto& entry : fs::recursive_directory_iterator(path_directory)) {
        if (fs::is_regular_file(entry)) {
            string path = entry.path().generic_string();
            pool.enqueue([path, &tr, &flag, &coutMutex]() { readMatch(path, tr, flag, coutMutex); });
        }
    }
    pool.~ThreadPool();

    clock_t end = clock();
    cout << "»¨·ÑÁË" << (double)(end - start) / CLOCKS_PER_SEC << "Ãë" << endl;

    return 0;
}
