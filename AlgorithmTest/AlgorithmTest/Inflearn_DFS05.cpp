#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

struct StudyTime
{
	int start;
	int end;
	int value;
};

int N, M, R, _result;
StudyTime study[1000];
vector<StudyTime> _StudyVec;
int dp[1000];

void DP()
{
	for (int i = 0; i < M; i++)
	{
		dp[i] = _StudyVec[i].value;
		for (int j = i - 1; j >= 0; j--)
		{
			if (_StudyVec[j].end + R <= _StudyVec[i].start && dp[i] < dp[j] + _StudyVec[i].value)
			{
				dp[i] = dp[j] + _StudyVec[i].value;
			}
		}
		_result = max(_result, dp[i]);
	}
}

int main()
{
	ios::sync_with_stdio(false);
	cin.tie(NULL);
	cout.tie(NULL);

	_result = -1;

	cin >> N >> M >> R;
	for (int i = 0; i < M; i++)
	{
		int start, end, value;
		cin >> start >> end >> value;
		StudyTime study;
		study.start = start;
		study.end = end;
		study.value = value;

		_StudyVec.push_back(study);
	}
	
	// end타임 오름차 순으로 정렬
	sort(_StudyVec.begin(), _StudyVec.end(), [](StudyTime a, StudyTime b) {
		return a.end < b.end;
		});

	DP();

	printf("%d\n", _result);

	return 0;
}