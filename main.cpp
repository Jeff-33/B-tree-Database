#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>

FILE* fo; // ȫ�ֱ���fo

/** �궨�� **/
#define Page_size 4*1024 // ҳ��С��Ϊ4KB
#define Index_page_size 500 // ����ҳ�����500��key
#define Kid_page_size 7 // Ҷ��ҳ�����7��key

using namespace std;

/* �ļ�ͷ�ṹ�� */
typedef struct {
	int steps; // B+���Ľ���
	unsigned int pageSize; // ҳ��С
	unsigned int pageCount; // �Ѵ�ҳ������
	unsigned int rootPagePla; // b+����ҳ������λ�ã�����ֵ��������ļ�ͷ����ƫ��ֵ��
}File_head;

/* ����ҳ�ṹ�� */
typedef struct {
	int pageType; // ҳ�����ͣ������ж�������ҳ����������ҳ
	int keyCount; // �Ѵ�key������
	unsigned int parentPagePla; // ��ҳ������λ��
	unsigned int key[Index_page_size + 1]; // ����ֵ
	unsigned int kidPagePla[Index_page_size + 1]; // ��ҳ������λ��
}Index_page;

/* ���ݽṹ�� */
typedef struct {
	char c1[129];
	char c2[129];
	char c3[129];
	char c4[81];
}Data;

/* ����ҳ�ṹ�� */
typedef struct {
	int pageType; // ҳ�����ͣ������ж�������ҳ����������ҳ
	int keyCount; // �Ѵ�key������
	bool isIncInsert; // �Ƿ��������
	bool isDecInsert; // �Ƿ�ݼ�����
	unsigned int parentPagePla; // ��ҳ������λ��
	unsigned int prevPagePla; // ǰһ��ҳ������λ��
	unsigned int nextPagePla; // ��һ��ҳ������λ��
	unsigned int key[Kid_page_size + 1]; // һ�����ݵ�keyֵ
	Data data[Kid_page_size + 1]; // һ�����ݵ�dataֵ
}Data_page;

/* ���ҽ���ṹ�� */
typedef struct {
	unsigned int pla; // ָ���λ��
	int index; // ��ҳ�еĹؼ������
	bool isFound; // �Ƿ��ҵ�
} Result;


/* �ӿ� */
bool insertData(Data data, unsigned int key); // ���뵥������
int loadCsvData(string csv_file_name); // ���ļ�����ķ�ʽ�����������
int searchBTIndex(Index_page* btPageBuf, unsigned int key); // ����key������ҳ�е�λ��
void searchDataPage(unsigned int page__pos, unsigned int key, Result* result); // ��������ҳ
bool selectData(unsigned int key, vector<bool> field); // ������ֵ��ѯ����
void selectRangeData(unsigned int left, unsigned int right, vector<bool> field); // ����������Χɨ���ѯ����
int outputCsvData(unsigned int left, unsigned int right, string csv_file_name); // ����������Χɨ��Ĳ�ѯ��������


void Stringsplit(string str, const char split, vector<string>& res) // �ָ��ַ���
{
	istringstream iss(str);	// ������
	string token;			// ���ջ�����
	while (getline(iss, token, split))	// ��splitΪ�ָ���
	{
		res.push_back(token);
	}
}

/* ���������ļ� */
void createDataFile() {
	fo = fopen("dataRecord.ibd", "rb");
	if (fo == NULL) {

		unsigned int page_pos;

		// ���벢��ʼ��ҳ
		File_head* fileHead = (File_head*)malloc(sizeof(File_head)); memset(fileHead, 0, sizeof(File_head));
		Index_page* btPage = (Index_page*)malloc(sizeof(Index_page)); memset(btPage, 0, sizeof(Index_page));
		Data_page* dataPage = (Data_page*)malloc(sizeof(Data_page)); memset(dataPage, 0, sizeof(Data_page));


		// ��mode��ʽ���ļ�
		fo = fopen("dataRecord.ibd", "wb+");

			/* ��ʼ��fileHead */
			fileHead->steps = Kid_page_size;
		fileHead->pageSize = Page_size;
		fileHead->pageCount = 3;
		fileHead->rootPagePla = 0;

		// �ļ�ĩβ��ʼд
		fseek(fo, 0 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(fileHead, sizeof(File_head), 1, fo);

		/** ��ʼ��fileHeadָ��ĵ�һ��btPage **/
		btPage->pageType = 1;
		btPage->keyCount = 1;
		btPage->key[0] = 0;
		// �ļ�ĩβ��ʼд
		fseek(fo, 1 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(btPage, sizeof(Index_page), 1, fo);

		fileHead->rootPagePla = page_pos;

		/** ��ʼ��btPageָ��ĵ�һ��dataPage **/

		dataPage->pageType = 2;
		dataPage->keyCount = 0;
		dataPage->isIncInsert = true;
		dataPage->isDecInsert = true;
		dataPage->parentPagePla = page_pos;
		// �ļ�ĩβ��ʼд
		fseek(fo, 2 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(dataPage, sizeof(Data_page), 1, fo);
		btPage->kidPagePla[0] = page_pos;

		// ָ��λ�ÿ�ʼд
		fseek(fo, 0, SEEK_SET);
		fwrite(fileHead, sizeof(File_head), 1, fo);

		// ָ��λ�ÿ�ʼд
		fseek(fo, fileHead->rootPagePla, SEEK_SET);
		fwrite(btPage, sizeof(Index_page), 1, fo);


		fclose(fo);
		free(fileHead);
		free(btPage);
		free(dataPage);
	}

	else {
		fclose(fo);
	}
}

/* �ж���Ҫ��Щ�ֶ� */
vector<bool> fieldChoice(string str) {
	string set[5] = { "a","c1","c2","c3","c4" };
	vector<bool> field(5);
	if (str == "*") {
		for (int i = 0; i < 5; i++) field[i] = true;
	}
	vector<string> fieldList;
	Stringsplit(str, ',', fieldList);	// ���Ӵ���ŵ�fieldList��
	for (int i = 0; i < fieldList.size(); i++) {
		for (int j = 0; j < 5; j++) {
			if (fieldList[i] == set[j])
				field[j] = true;
		}
	}
	return field;
}

int main() {
	createDataFile(); // û�������ļ�ʱ����Ҫ���������ļ�
	while (true)
	{
		cout << '>';
		string str;
		getline(cin, str);
		vector<string> strList;
		Stringsplit(str, ' ', strList);
		string menu = strList[0];

		/* �������� */
		if (menu.compare("insert") == 0) {

			if (strList.size() == 6) { // ���뵥������
				Data data;

				unsigned int key = stoi(strList[1]);
				strcpy(data.c1, strList[2].c_str());
				strcpy(data.c2, strList[3].c_str());
				strcpy(data.c3, strList[4].c_str());
				strcpy(data.c4, strList[5].c_str());

				clock_t start, end;
				start = clock();

				if (insertData(data, key)) {
					end = clock();
					cout << "ִ�в��뵥������:" << key << "����ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;
				}
			}
			else if (strList.size() == 2) { // ���ļ�����ķ�ʽ�����������
				clock_t start, end;
				start = clock();

				cout << "��ʼ�������ݣ����Ժ󡣡���" << endl;
				int cnt = loadCsvData(strList[1]);

				end = clock();
				cout << "����������ɣ�" << cnt << "���ݱ����룬��ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;
			}
			else {
				cout << "���������������������" << endl;
			}
		}

		/* ��ѯ���� */
		else if (menu.compare("select") == 0) {
			if (strList.size() == 4) { // ������ֵ��ѯ����
				vector<bool> field = fieldChoice(strList[1]);
				string str = strList[3];
				vector<string> strList;
				Stringsplit(str, '=', strList);

				clock_t start, end;
				start = clock();

				if (selectData(stoi(strList[1]), field)) {
					end = clock();
					cout << "ִ�в������ݲ�������ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;
				}
				else {
					end = clock();
					cout << "emmm�Ҳ����������ݣ���ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;
				}
			}
			else if (strList.size() == 8) { // ����������Χɨ���ѯ����
				vector<bool> field = fieldChoice(strList[1]);

				clock_t start, end;
				start = clock();

				selectRangeData(stoi(strList[5]), stoi(strList[7]), field);

				end = clock();
				cout << "ִ��������Χɨ���ѯ����ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;
			}

			else if (strList.size() == 9) { // ����������Χɨ��Ĳ�ѯ��������
				cout << "��ʼ�������ݣ����Ժ󡣡���" << endl;
				clock_t start, end;
				start = clock();
				outputCsvData(stoi(strList[6]), stoi(strList[8]), strList[2]);
				end = clock();
				cout << "���ݵ�����ɣ���ʱ��" << float(end - start) / CLOCKS_PER_SEC << " ��" << endl;  //���ʱ�䣨��λ����
			}
			else {
				cout << "���������������������" << endl;
			}

		}

		else if (menu.compare("help") == 0) { //help�˵�
			printf(
				"select <items> where a=?           ------------ ��ѯ��ʽ\n"\
				"insert a c1 c2 c3 c4               ------------ �������ݸ�ʽ\n "\
				"reset                              ------------ �������ݿ�\n"\
				"shutdown                           ------------ ͣ������\n"\
				"select <items> where a between ? and ?      --- ��ѯС��Χ���\n"\
				"select into xxx.csv where a between ? and ?  -- ���ݵ���\n"
			);
		}


		else if (menu.compare("reset") == 0) {
			remove("dataRecord.ibd");
			cout << "���ݿ�������~" << endl;

			createDataFile();
		}

		/* �رճ��� */
		else if (menu.compare("shutdown") == 0) {
			return 0;
		}

		else {
			cout << "���������������������" << endl;
		}
	}
}
