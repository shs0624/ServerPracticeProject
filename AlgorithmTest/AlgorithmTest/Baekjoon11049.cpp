//#include <iostream>
//#include <vector>
//#include <algorithm>
//using namespace std;
//
//struct Matrix
//{
//	int r;
//	int c;
//};
//
//int N, _result;
//Matrix _MatrixArr[501];
//unsigned int _dp[501][501];
//
//void Solution()
//{
//	// term = term만큼의 행렬을 묶어서 연산하겠다.
//	for (int term = 1; term < N; term++)
//	{
//		// i = 곱셈을 시작하는 인덱스
//		for (int i = 1; term + i <= N; i++)
//		{
//			_dp[i][term + i] = 2147000000;
//			// 시작점 i부터 i+term까지 탐색하며 i부터 i+term까지의 행렬곱 최솟값을 구한다.
//			// 구한 값을 _dp[시작][끝]에 저장
//			for (int j = i; j <= term + i; j++)
//			{
//				_dp[i][term + i] = min(_dp[i][term + i],
//					_dp[i][j] + _dp[j + 1][term + i] + _MatrixArr[i].r * _MatrixArr[j].c * _MatrixArr[term + i].c);
//			}
//		}
//	}
//
//	_result = _dp[1][N];
//}
//
//int main()
//{
//	ios::sync_with_stdio(false);
//	cin.tie(NULL);
//	cout.tie(NULL);
//
//	_result = 0xffffffff;
//
//	cin >> N;
//	for (int i = 1; i <= N; i++)
//	{
//		cin >> _MatrixArr[i].r >> _MatrixArr[i].c;
//	}
//
//	Solution();
//
//	cout << _result << endl;
//
//	return 0;
//}