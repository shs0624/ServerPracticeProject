//#include <iostream>
//#include <queue>
//#include <algorithm>
//using namespace std;
//
//int N, D, K, answer;
//int taste[30001][15];
//int bit[30001];
//char check[15];
//
//void DFS(int idx, int count, int bitSum)
//{
//    if (count == K)
//    {
//        int cnt = 0;
//        for (int i = 1; i <= N; i++)
//        {
//            if ((bit[i] & bitSum) == bit[i])
//                cnt++;
//        }
//        answer = max(cnt, answer);
//    }
//    else
//    {
//        for (int i = idx; i <= D; i++)
//        {
//            DFS(i + 1, count + 1, bitSum | (1 << i));
//        }
//    }
//}
//
//void Solution()
//{
//    DFS(1, 0, 0);
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//    cin >> N >> D >> K;
//    for (int i = 1; i <= N; i++)
//    {
//        int temp;
//        cin >> temp;
//        for (int j = 0; j < temp; j++)
//            cin >> taste[i][j];
//
//        for (int j = 0; j < temp; j++)
//        {
//            if (taste[i][j] != 0)
//                bit[i] |= 1 << taste[i][j];
//        }
//    }
//
//    cin.ignore();
//    answer = 0;
//
//    Solution();
//
//    cout << answer;
//
//    return 0;
//}