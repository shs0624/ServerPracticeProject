//#include <iostream>
//#include <vector>
//#include <unordered_map>
//#include <algorithm>
//using namespace std;
//
//struct gameData
//{
//    char team;
//    int power;
//
//    bool operator<(const gameData& b) const
//    {
//        return power < b.power;
//    }
//};
//
//int N;
//vector<pair<gameData, int>> vec;
//unordered_map<char, int> teamMap;
//int score[200001];
//
//void Solution()
//{
//    sort(vec.begin(), vec.end());
//
//    int Sum = 0;
//    int j = 0;
//
//    for (int i = 0; i < vec.size(); i++)
//    {
//        while (vec[j].first.power < vec[i].first.power)
//        {
//            Sum += vec[j].first.power;
//            teamMap[vec[j].first.team] += vec[j].first.power;
//            j++;
//        }
//
//        score[vec[i].second] = Sum - teamMap[vec[i].first.team];
//    }
//
//    for (int i = 0; i < vec.size(); i++)
//    {
//        cout << score[i] << endl;
//    }
//}
//
//int main()
//{
//    ios_base::sync_with_stdio(false);
//
//    cin >> N;
//    for (int i = 0; i < N; i++)
//    {
//        char team;
//        int power;
//
//        cin >> team >> power;
//        vec.push_back({ { team, power }, i });
//    }
//
//    Solution();
//
//    return 0;
//}