//#include <iostream>
//using namespace std;
//
//int n, m, k;
//int setArr[10001];
//int cost[10001];
//
//void Union(int to, int from)
//{
//	for (int i = 1; i <= n; i++)
//	{
//		if (setArr[i] == from)
//			setArr[i] = to;
//	}
//}
//
//int Find(int target)
//{
//	if (setArr[target] != target)
//		return Find(setArr[target]);
//	else
//		return cost[target];
//}
//
//void SetCommunity(int c1, int c2)
//{
//	int c1Cost = Find(c1);
//	int c2Cost = Find(c2);
//
//	if (c1Cost < c2Cost)
//		Union(setArr[c1], setArr[c2]);
//	else
//		Union(setArr[c2], setArr[c1]);
//}
//
//int GetResult()
//{
//	int result = 0;
//	for (int i = 1; i <= n; i++)
//	{
//		if(setArr[i] == i)
//			result += cost[i];
//	}
//
//	return result;
//}
//
//int main()
//{
//	ios::sync_with_stdio(false);
//	cin.tie(NULL);
//	cout.tie(NULL);
//
//	cin >> n >> m >> k;
//	for (int i = 1; i <= n; i++)
//	{
//		cin >> cost[i];
//		setArr[i] = i;
//	}
//
//	// 친구관계
//	for (int i = 0; i < m; i++)
//	{
//		int c1, c2;
//		cin >> c1 >> c2;
//
//		SetCommunity(c1, c2);
//	}
//
//	int result = GetResult();
//
//	if (result <= k)
//		cout << result << endl;
//	else
//		cout << "Oh no" << endl;
//
//	return 0;
//}