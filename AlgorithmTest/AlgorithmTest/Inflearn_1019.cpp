//#include <iostream>
//#include <map>
//#include <algorithm>
//using namespace std;
//
//int N, answer;
//long long K, sum;
//int work[200001];
//map<long long, int> workMap;
//
//int Solution()
//{
//    if (K >= sum)
//        return -1;
//
//    int loopN = N;
//    int prevLoop = 0;
//
//    for (auto it = workMap.begin(); it != workMap.end(); it++)
//    {
//        long long currentLoop = (*it).first;
//        int count = (*it).second;
//
//        long long diff = currentLoop - prevLoop;
//
//        if (K >= (diff * loopN))
//        {
//            K -= (diff * loopN);
//
//            loopN -= count;
//            prevLoop = currentLoop;
//        }
//        else
//        {
//            long long left = K % loopN;
//
//            for (int i = 0; i < N; i++)
//            {
//                if (work[i] <= prevLoop)
//                    continue;
//
//                if (left == 0)
//                    return i + 1;
//
//                left--;
//            }
//        }
//    }
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    cin >> N;
//    for (int i = 0; i < N; i++)
//    {
//        cin >> work[i];
//        sum += work[i];
//
//        workMap[work[i]]++;
//    }
//
//    cin >> K;
//    cin.ignore();
//
//    answer = Solution();
//
//    cout << answer;
//
//    return 0;
//}