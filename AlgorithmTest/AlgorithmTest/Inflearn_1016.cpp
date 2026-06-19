//#include <iostream>
//#include <vector>
//#include <algorithm>
//using namespace std;
//
////struct StudyTime
////{
////    int st;
////    int et;
////    int value;
////};
////
////StudyTime studyArr[1001];
////char check[1001];
////char timeTable[1000001];
////
////int N, M, R;
////int answer;
////
////// TimeLimit
////void DFS(int idx)
////{
////    if (idx == M)
////    {
////        int result = 0;
////        //ÃøÁ¤
////        for (int i = 0; i < M; i++)
////        {
////            if (check[i] == 1)
////            {
////                result += studyArr[i].value;
////            }
////        }
////
////        answer = max(result, answer);
////        return;
////    }
////
////    int st = studyArr[idx].st;
////    int et = studyArr[idx].et;
////    
////    char flag = 0;
////    for (int i = st; i <= et + R - 1; i++)
////    {
////        if (timeTable[i] == 1)
////        {
////            flag = 1;
////            break;
////        }
////    }
////
////    if (flag == 1)
////    {
////        DFS(idx + 1);
////    }
////    else
////    {
////        check[idx] = 1;
////        for (int i = st; i <= et + R - 1; i++)
////        {
////            timeTable[i] = 1;
////        }
////
////        DFS(idx + 1);
////
////        check[idx] = 0;
////        for (int i = st; i <= et + R - 1; i++)
////        {
////            timeTable[i] = 0;
////        }
////
////        DFS(idx + 1);
////    }
////}
//
////int main() {
////    ios_base::sync_with_stdio(false);
////
////    cin >> N >> M >> R;
////    cin.ignore();
////    for (int i = 0; i < M; i++)
////    {
////        cin >> studyArr[i].st >> studyArr[i].et >> studyArr[i].value;
////    }
////
////    answer = 0;
////
////    DFS(0);
////
////    cout << answer;
////
////    return 0;
////}
//
//struct StudyTime
//{
//    int st;
//    int et;
//    int value;
//
//    bool operator <(const StudyTime& st) const {
//        return et < st.et;
//    }
//};
//
//
//vector<StudyTime> studyVec;
//int dp[1001];
//
//int N, M, R;
//int answer;
//
//void Solution()
//{
//    for (int i = 0; i < M; i++)
//    {
//        dp[i] = studyVec[i].value;
//        for (int j = i - 1; j >= 0; j--)
//        {
//            if (studyVec[j].et + R <= studyVec[i].st)
//            {
//                dp[i] = max(dp[j] + studyVec[i].value, dp[i]);
//            }
//        }
//    }
//
//    for (int i = 0; i < M; i++)
//    {
//        answer = max(answer, dp[i]);
//    }
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    cin >> N >> M >> R;
//    cin.ignore();
//    for (int i = 0; i < M; i++)
//    {
//        StudyTime study;
//        cin >> study.st >> study.et >> study.value;
//        studyVec.push_back(study);
//    }
//    
//    sort(studyVec.begin(), studyVec.end());
//
//    answer = 0;
//
//    Solution();
//
//    cout << answer;
//
//    return 0;
//}