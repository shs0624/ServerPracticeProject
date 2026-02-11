#include <iostream>
using namespace std;

int cost[10001];
int communication[10001][10001];

void CommunitySet(int c1, int c2)
{
	// 자기 자신
	if (c1 == c2)
		return;

	if (cost[c1] < cost[c2])
		cost[c2] = 0;
	else
		cost[c1] = 0;
}

int main()
{
	ios::sync_with_stdio(false);
	cin.tie(NULL);
	cout.tie(NULL);

	int n, m, k;
	int min = 2147483647;
	cin >> n >> m >> k;
	for (int i = 1; i <= n; i++)
	{
		cin >> cost[i];
		if (cost[i] <= min)
			min = cost[i];
	}

	// 친구관계
	for (int i = 0; i < m; i++)
	{
		int c1, c2;
		cin >> c1 >> c2;

		CommunitySet(c1, c2);
	}

	int result = 0;
	for (int i = 1; i <= n; i++)
	{
		result += cost[i];
	}

	// 모두가 묶이면 0인상황
	if (result == 0)
		result = min;

	if (result <= k)
		cout << result << endl;
	else
		cout << "Oh no" << endl;

	return 0;
}