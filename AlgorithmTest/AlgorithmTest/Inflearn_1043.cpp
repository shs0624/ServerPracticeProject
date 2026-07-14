#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
using namespace std;

string str;
unordered_map<char, int> _cMap;
unordered_map<char, int> _usedMap;

void Solution()
{
    string result;
    for (int i = 0; i < str.size(); i++)
    {
        if (str[i] >= 'a' && str[i] <= 'z')
            str[i] -= 32;

        _cMap[str[i]]++;
    }

    for (int i = 0; i < str.size(); i++)
    {
        _cMap[str[i]]--;

        if (_usedMap[str[i]] == 1)
            continue;

        if (result.empty())
        {
            result.push_back(str[i]);
        }
        else
        {
            while (result.back() > str[i] && _cMap[result.back()] > 0)
            {
                _usedMap[result.back()]--;
                result.pop_back();
            }

            result.push_back(str[i]);
        }

        _usedMap[str[i]]++;
    }

    cout << result << endl;
}


int main()
{
    ios_base::sync_with_stdio(false);

    cin >> str;

    Solution();

    return 0;
}