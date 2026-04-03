#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int A, B, C, result;

void Solution()
{
	int revenue;
	if (B >= C)
	{
		result = -1;
		return;
	}

	revenue = C - B;
	result = (A / revenue) + 1;
	return;
}

int main()
{
	ios::sync_with_stdio(false);
	cin.tie(NULL);
	cout.tie(NULL);

	cin >> A >> B >> C;

	Solution();

	cout << result << endl;

	return 0;
}