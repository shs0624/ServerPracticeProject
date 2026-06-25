#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int N;
int dp[101];

void Solution(vector<int>& vec)
{
    dp[0] = vec[0];
    dp[1] = vec[1];
    for (int i = 2; i < N; i++)
    {
        // 가장 빠른애가 두번
        // 기존 거 + 새로운 놈 데려다주고, 돌아오는 값
        int num1 = dp[i - 1] + vec[0] + vec[i];
        // 가장 느린 둘이 한 번에
        // 기존 거 + 가장 빠른 둘이 가고, 느린 둘이 가고, 두번째로 빠른 애가 오고
        int num2 = dp[i - 2] + vec[i] + (2 * vec[1]) + vec[0];

        dp[i] = min(num1, num2);
    }

    cout << dp[N - 1];
}

int main()
{
    ios_base::sync_with_stdio(false);
    vector<int> vec;

    cin >> N ;

    for (int i = 0; i < N; i++)
    {
        int num;
        cin >> num;
        vec.push_back(num);
    }

    sort(vec.begin(), vec.end());

    Solution(vec);

    return 0;
}