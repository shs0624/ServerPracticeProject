#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
using namespace std;

int N;
char Essential;

void Solution(vector<string>& vec)
{
    unordered_map<char, int> um;
    vector<char> printVec;
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < vec[i].size(); j++)
        {
            char temp = vec[i][j];
            if (temp >= 'a' && temp <= 'z')
                temp -= 32;

            if (um.find(temp) == um.end())
                printVec.push_back(temp);

            um[temp]++;
        }

        if (um.find(Essential) != um.end())
        {
            for (int j = 0; j < printVec.size(); j++)
                cout << printVec[j];

            cout << endl;
        }

        um.clear();
        printVec.clear();
    }
}

int main() 
{
    ios_base::sync_with_stdio(false);
    vector<string> vec;
    string Arr[101];
    
    cin >> N >> Essential;

    if (Essential >= 'a' && Essential <= 'z')
        Essential -= 32;

    for (int i = 0; i < N; i++)
    {
        string str;
        cin >> str;
        vec.push_back(str);
    }

    Solution(vec);

    return 0;
}