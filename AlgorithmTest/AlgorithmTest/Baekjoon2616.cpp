#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int N, M = 0;
int Count[50001];
int dp_Sum[50001];
int dp_pair[50001];
int dp_double[50001];
int dp[50001];
int _result;

void Solution()
{
	dp_pair[1] = Count[1];
	dp_double[1] = Count[1];
	dp[1] = Count[1];

	for (int i = 1; i <= M; i++)
	{
		dp_Sum[i] = dp_Sum[i - 1] + Count[i];
		dp_pair[i] = dp_pair[i - 1] + Count[i];
		dp_double[i] = dp_double[i - 1] + Count[i];
		dp[i] = dp[i - 1] + Count[i];
	}

	for (int i = M; i <= N; i++)
	{
		dp_Sum[i] = dp_Sum[i - 1] + Count[i] - Count[i - M];
		dp_pair[i] = max(dp_pair[i - 1], dp_Sum[i]);
		// m개씩 묶을 때, 해당 수까지의 최대 pair값 저장
		dp_double[i] = max(dp_double[i - 1], dp_pair[i - M] + dp_Sum[i]);
		// m개씩 2조로 묶는 경우 그 수까지의 조합 중 최대치
		dp[i] = max(dp[i - 1], dp_double[i - M] + dp_Sum[i]);
		// m개씩 3조로 묶는 경우.
	}

	_result = dp[N];
}

int main()
{
	ios::sync_with_stdio(false);
	cin.tie(NULL);
	cout.tie(NULL);

	cin >> N;
	for (int i = 1; i <= N; i++)
		cin >> Count[i];
	cin >> M;

	Solution();

	cout << _result << endl;

	return 0;
}