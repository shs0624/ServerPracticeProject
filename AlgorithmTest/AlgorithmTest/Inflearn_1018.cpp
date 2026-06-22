//#include <iostream>
//#include <string>
//#include <unordered_map>
//#include <algorithm>
//using namespace std;
//
//string S, T;
//int answer;
//
//void Solution()
//{
//    string comp;
//    comp = T;
//    sort(comp.begin(), comp.end());
//
//    unordered_map<char, int> compareUm;
//    for (int i = 0; i < T.size(); i++)
//        compareUm[T[i]]++;
//
//    unordered_map<char, int> tempUm;
//    for (int i = 0; i < T.size() - 1; i++)
//        tempUm[S[i]]++;
//
//    int idx = 0;
//    for (int i = T.size() - 1; i < S.size(); i++)
//    {
//        tempUm[S[i]]++;
//        if (compareUm == tempUm)
//        {
//            answer++;
//        }
//        tempUm[S[idx++]]--;
//        if (tempUm[S[idx - 1]] == 0)
//            tempUm.erase(S[idx - 1]);
//    }
//}
//
////// Ç®ÀÌ - Timeout...
////void Solution()
////{
////    string comp;
////    comp = T;
////    sort(comp.begin(), comp.end());
////
////    unordered_map<char, int> um;
////    for (int i = 0; i < T.size(); i++)
////        um[T[i]]++;
////
////    unordered_map<char, int> tempUm;
////    string temp;
////    for (int i = 0; i <= S.size() - T.size(); i++)
////    {
////        if (um.find(S[i]) != um.end())
////        {
////            for (int j = 0; j < T.size(); j++)
////            {
////                if (um.find(S[i + j]) == um.end())
////                {
////                    break;
////                }
////                tempUm[S[i + j]]++;
////            }
////
////            if (tempUm == um)
////                answer++;
////
////            tempUm.clear();
////
////            //temp = S.substr(i, T.size());
////
////            //sort(temp.begin(), temp.end());
////            //if (comp.compare(temp) == 0)
////            //{
////            //    answer++;
////            //}
////        }
////    }
////}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    cin >> S;
//    cin >> T;
//    cin.ignore();
//
//    Solution();
//
//    cout << answer;
//
//    return 0;
//}