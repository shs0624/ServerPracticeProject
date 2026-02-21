//#include <iostream>
//#include <vector>
//#include <algorithm>
//using namespace std;
//
//int N, M;
//char board[1001][1001];
//int dp[1001][1001];
//int _result;
//
//void Solution()
//{
//	for (int i = 1; i <= N; i++)
//	{
//		for (int j = 1; j <= M; j++)
//		{
//			if (board[i][j] == '1')
//			{
//				if (dp[i - 1][j - 1] > 0 && dp[i][j - 1] > 0 && dp[i - 1][j] > 0)
//				{
//					int value = min({ dp[i - 1][j - 1], dp[i][j - 1], dp[i - 1][j] });
//					dp[i][j] = value + 1;
//
//					_result = max(value + 1, _result);
//				}
//				else
//				{
//					dp[i][j] = 1;
//					_result = max(1, _result);
//				}
//			}
//			else
//			{
//				dp[i][j] = 0;
//			}
//		}
//	}
//
//	_result = _result * _result;
//}
//
//int main()
//{
//	ios::sync_with_stdio(false);
//	cin.tie(NULL);
//	cout.tie(NULL);
//
//	cin >> N >> M;
//	for (int i = 1; i <= N; i++)
//	{
//		for (int j = 1; j <= M; j++)
//		{
//			cin >> board[i][j];
//		}
//	}
//
//	Solution();
//
//	cout << _result << endl;
//
//	return 0;
//}