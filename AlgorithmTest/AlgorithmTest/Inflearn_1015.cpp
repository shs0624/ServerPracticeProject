//#include <iostream>
//#include <algorithm>
//using namespace std;
//
//int N;
//int check[17];
//int arr[17][2];
//int answer;
//
//// 다시풀기
//void DFS2(int idx, int bCount, int wCount, int bResult, int wResult)
//{
//    if (idx == N)
//    {
//        answer = min(abs(bResult - wResult), answer);
//    }
//
//    if (bCount < N / 2)
//    {
//        DFS2(idx + 1, bCount + 1, wCount, bResult + arr[idx][1], wResult);
//    }
//   
//    if (wCount < N / 2)
//    {
//        DFS2(idx + 1, bCount, wCount + 1, bResult, wResult + arr[idx][0]);
//    }
//}
//
//// 여러번 실패해서 답봤음
//// 조합을 구하는 DFS L = level, S = 첫 시작 숫자
//void DFS(int L, int s)
//{
//    if (L == N / 2)
//    {
//        int blackResult = 0, whiteResult = 0;
//        for (int i = 0; i < N; i++)
//        {
//            if (check[i] == 0)
//                whiteResult += arr[i][0];
//            else
//                blackResult += arr[i][1];
//        }
//
//        answer = min(abs(blackResult - whiteResult), answer);
//        return;
//    }
//
//    for (int i = s; i < N; i++)
//    {
//        check[i] = 1;
//        DFS(L + 1, i + 1);
//        check[i] = 0;
//    }
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    cin >> N;
//    cin.ignore();
//    for (int i = 0; i < N; i++)
//    {
//        cin >> arr[i][0] >> arr[i][1];
//    }
//
//    answer = 100001;
//
//    //DFS(0, 0);
//    DFS2(0, 0, 0, 0, 0);
//
//    cout << answer;
//
//    return 0;
//}