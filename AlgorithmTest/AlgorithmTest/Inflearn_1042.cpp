#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

int n, maxValue;
int seat[101];
int dist[101];

void Solution()
{
    int p = 1000;
    for (int i = 0; i < n; i++)
    {
        if (seat[i] == 1)
            p = 0;
        else if (seat[i] == 0)
        {
            dist[i] = ++p;
        }
    }

    p = 1000;
    for (int i = n - 1; i >= 0; i--)
    {
        if (seat[i] == 1)
            p = 0;
        else if (seat[i] == 0)
        {
            dist[i] = min(dist[i], ++p);
        }
    }

    for (int i = 0; i < n; i++)
    {
        if (dist[i] != 0)
            maxValue = max(dist[i], maxValue);
    }

    cout << maxValue;
}

// ¼öÁ¤ Àü
//void Solution()
//{
//    int distIdx = -1;
//    for (int i = 0; i < n; i++)
//    {
//        if (seat[i] == 1)
//            distIdx = i;
//        else if (seat[i] == 0 && distIdx != -1)
//        {
//            dist[i] = i - distIdx;
//        }
//    }
//
//    distIdx = -1;
//    for (int i = n - 1; i >= 0; i--)
//    {
//        if (seat[i] == 1)
//            distIdx = i;
//        else if (seat[i] == 0 && distIdx != -1)
//        {
//            if (dist[i] != 0)
//                dist[i] = min(dist[i], distIdx - i);
//            else
//                dist[i] = distIdx - i;
//        }
//    }
//
//    for (int i = 0; i < n; i++)
//    {
//        if (dist[i] != 0)
//            maxValue = max(dist[i], maxValue);
//    }
//
//    cout << maxValue;
//}

int main()
{
    ios_base::sync_with_stdio(false);

    int distIdx = -1;
    maxValue = 0;

    cin >> n;
    for (int i = 0; i < n; i++)
    {
        cin >> seat[i];
    }

    Solution();

    return 0;
}