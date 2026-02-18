//#include <iostream>
//using namespace std;
//
//int N, limit, result;
//int check[16];
//pair<int, int> _PowerArr[16];
//
////해설 - 정석적인 DFS. L = Level, S = 가지의 첫 시작 숫자
//void DFS_Answer(int L, int S)
//{
//	if (L == N / 2)
//	{
//		// 결과
//		int resultArr[2][16];
//		int wResult = 0, bResult = 0;
//		for (int i = 0; i < N; i++)
//		{
//			if (check[i] == 0)
//				wResult += _PowerArr[i].first;
//			else
//				bResult += _PowerArr[i].second;
//		}
//
//		result = min(result, abs(wResult - bResult));
//	}
//	else
//	{
//		for (int i = S; i < N; i++)
//		{
//			check[i] = 1;
//			DFS_Answer(L + 1, i + 1);
//			check[i] = 0;
//		}
//	}
//}
//
//// 내 풀이
//void DFS(int idx, int wCount, int bCount, int wSum, int bSum)
//{
//	if (idx >= N)
//	{
//		// 결과
//		int diff = abs(wSum - bSum);
//
//		if (result < diff)
//			result = result;
//		else
//			result = diff;
//	}
//
//	if (wCount < limit)
//		DFS(idx + 1, wCount + 1, bCount, wSum + _PowerArr[idx].first, bSum);
//
//	if (bCount < limit)
//		DFS(idx + 1, wCount, bCount + 1, wSum, bSum + _PowerArr[idx].second);
//}
//
//int main()
//{
//	ios::sync_with_stdio(false);
//	cin.tie(NULL);
//	cout.tie(NULL);
//
//	result = 2147483640;
//
//	cin >> N;
//	limit = N / 2;
//	for (int i = 0; i < N; i++)
//	{
//		int b, w;
//		cin >> w >> b;
//		_PowerArr[i].first = w;
//		_PowerArr[i].second = b;
//	}
//
//	//DFS(0, 0, 0, 0, 0);
//	DFS_Answer(0, 0);
//	printf("%d\n", result);
//
//	return 0;
//}