//#include <iostream>
//#include <string>
//#include <unordered_map>
//#include <algorithm>
//using namespace std;
//
//string S;
//int N, M, R;
//int answer;
//
//// 강의
//void Solution()
//{
//    int result = 0;
//    for (int i = 0; i < S.size(); i++)
//    {
//        if (S[i] >= '0' && S[i] <= '9')
//        {
//            result = result * 10 + (S[i] - '0');
//        }
//        else
//        {
//            answer += result;
//            result = 0;
//        }
//    }
//
//    if (result != 0)
//        answer += result;
//}
//
//// 내 풀이
////void Solution()
////{
////    unordered_map<string, int> um;
////    string temp;
////    for (int i = 0; i < S.size(); i++)
////    {
////        if (S[i] >= '0' && S[i] <= '9')
////        {
////            if (temp.empty() && S[i] == '0')
////                continue;
////            temp.push_back(S[i]);
////        }
////        else
////        {
////            if (!temp.empty())
////            {
////                um[temp]++;
////            }
////            temp.clear();
////        }
////    }
////
////    if (!temp.empty())
////    {
////        um[temp]++;
////    }
////    temp.clear();
////
////    for (auto it = um.begin(); it != um.end(); it++)
////    {
////        int loop = (*it).second;
////        int num = stoi((*it).first);
////        for (int i = 0; i < loop; i++)
////            answer += num;
////    }
////}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    cin >> S;
//    cin.ignore();
//
//    answer = 0;
//
//    Solution();
//
//    cout << answer;
//
//    return 0;
//}