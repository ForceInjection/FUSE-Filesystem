#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * 编译和挂载文件系统说明
 *
 * 1. 编译文件系统：
 *    使用以下命令编译文件系统代码：
 *    gcc FS.c -o FS `pkg-config fuse --cflags --libs`
 *
 *    参数说明：
 *    - gcc FS.c: 编译 FS.c 文件。
 *    - -o FS: 指定输出文件名为 FS。
 *    - `pkg-config fuse --cflags --libs`: 自动获取 FUSE 库的编译和链接参数。
 *
 * 2. 挂载文件系统：
 *    使用以下命令挂载文件系统：
 *    ./FS -f Desktop/OS/mountpoint4
 *
 *    参数说明：
 *    - ./FS: 运行编译生成的文件系统程序。
 *    - -f: 在前台运行文件系统（调试模式），程序会输出日志信息。
 *    - Desktop/OS/mountpoint4: 挂载点路径，文件系统将挂载到该目录。
 *
 * 3. 挂载后的操作：
 *    挂载成功后，可以通过挂载点路径（如 Desktop/OS/mountpoint4）访问文件系统：
 *    - 创建文件: touch Desktop/OS/mountpoint4/test.txt
 *    - 读取文件: cat Desktop/OS/mountpoint4/test.txt
 *    - 删除文件: rm Desktop/OS/mountpoint4/test.txt
 *
 * 4. 卸载文件系统：
 *    使用以下命令卸载文件系统：
 *    fusermount -u Desktop/OS/mountpoint4
 *
 *    参数说明：
 *    - fusermount -u: 卸载文件系统。
 *    - Desktop/OS/mountpoint4: 挂载点路径。
 *
 * 5. 调试模式：
 *    如果使用 -f 参数在前台运行文件系统，程序会输出日志信息，方便调试。例如：
 *    OPEN /test.txt
 *    READ /test.txt
 *    CLOSE /test.txt
 *
 * 注意事项：
 * 1. 确保挂载点路径存在且为空目录。
 * 2. 如果挂载失败，检查是否有权限问题或路径错误。
 * 3. 调试完成后，可以去掉 -f 参数，让文件系统在后台运行。
 */

#define block_size 1024

/*
 * superblock - 文件系统超级块结构
 *
 * 功能：
 * 1. 存储文件系统的全局元数据信息。
 * 2. 管理数据块和 inode 的分配状态。
 *
 * 字段说明：
 * - datablocks: 数据块数组，存储文件系统的所有数据块。
 * - data_bitmap: 数据块位图，标识哪些数据块已被占用（'1'）或空闲（'0'）。
 * - inode_bitmap: inode 位图，标识哪些 inode 已被占用（'1'）或空闲（'0'）。
 *
 * 示例：
 * 假设文件系统刚初始化，调用 initialize_superblock() 后：
 * - spblock.data_bitmap 的所有位均为 '0'，表示所有数据块空闲。
 * - spblock.inode_bitmap 的所有位均为 '0'，表示所有 inode 空闲。
 *
 * 注意：
 * - 超级块是文件系统的核心数据结构，必须在文件系统初始化时调用 initialize_superblock() 方法。
 * - 数据块位图和 inode 位图的大小固定为 100 个字节。
 */
typedef struct superblock
{
	char datablocks[block_size * 100]; // 数据块数组，存储文件系统的所有数据块
	char data_bitmap[105];			   // 数据块位图，标识数据块的占用状态
	char inode_bitmap[105];			   // inode 位图，标识 inode 的占用状态
} superblock;

/*
 * inode - 文件系统索引节点结构
 *
 * 功能：
 * 1. 存储文件或目录的元数据信息。
 * 2. 管理文件数据块的分配和引用。
 *
 * 字段说明：
 * - datablocks: 数据块编号数组，存储文件数据所在的数据块。
 * - number: inode 的唯一标识符。
 * - blocks: 文件占用的数据块数量。
 * - size: 文件或目录的大小（以字节为单位）。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * inode 示例：
 * inode (test.txt):
 * - datablocks: [5, 6, 7]  // 文件数据存储在数据块 5、6、7 中
 * - number: 3              // inode 编号为 3
 * - blocks: 3              // 文件占用 3 个数据块
 * - size: 3072             // 文件大小为 3072 字节
 *
 * 注意：
 * - inode 是文件系统的核心数据结构，用于管理文件的元数据和数据块。
 * - 每个文件或目录都有一个唯一的 inode。
 */
typedef struct inode
{
	int datablocks[16]; // 数据块编号数组，存储文件数据所在的数据块
	int number;			// inode 的唯一标识符
	int blocks;			// 文件占用的数据块数量
	// int link;                    //==number of links
	int size; // 文件或目录的大小（以字节为单位）
} inode;

/*
 * filetype - 文件/目录元数据结构
 *
 * 功能：
 * 1. 存储文件或目录的元数据信息。
 * 2. 用于管理文件系统的树形结构。
 *
 * 字段说明：
 * - valid: 标识节点是否有效（1 表示有效，0 表示无效）。
 * - test: 保留字段，未使用。
 * - path: 文件或目录的完整路径。
 * - name: 文件或目录的名称。
 * - inum: 指向关联的 inode 结构。
 * - children: 子节点指针数组，用于存储目录的子文件或子目录。
 * - num_children: 子节点数量。
 * - num_links: 硬链接数。
 * - parent: 指向父目录的指针。
 * - type: 文件类型（如 "file" 或 "directory"）。
 * - permissions: 文件或目录的权限模式（如 0777）。
 * - user_id: 文件或目录的用户 ID。
 * - group_id: 文件或目录的组 ID。
 * - a_time: 最后访问时间。
 * - m_time: 最后修改时间。
 * - c_time: 最后状态更改时间。
 * - b_time: 创建时间。
 * - size: 文件或目录的大小（以字节为单位）。
 * - datablocks: 数据块编号数组，存储文件数据所在的数据块。
 * - number: 文件或目录的编号（唯一标识）。
 * - blocks: 文件占用的数据块数量。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 文件类型示例：
 * Filetype (Root):
 * - valid: 1
 * - path: "/"
 * - name: "/"
 * - type: "directory"
 * - permissions: 0777
 * - user_id: 1000
 * - group_id: 1000
 * - a_time: 1698765432
 * - m_time: 1698765432
 * - c_time: 1698765432
 * - b_time: 1698765432
 * - size: 4096
 * - number: 1
 * - blocks: 1
 * - children: [Filetype (home), Filetype (test.txt)]
 * - num_children: 2
 * - parent: NULL
 * - num_links: 2
 *
 * 注意：
 * - 文件类型和目录类型使用相同的结构体。
 * - 目录的 size 字段通常表示目录元数据的大小。
 */
typedef struct filetype
{
	int valid;					// 标识节点是否有效
	char test[10];				// 保留字段，未使用
	char path[100];				// 文件或目录的完整路径
	char name[100];				// 文件或目录的名称
	inode *inum;				// 指向关联的 inode 结构
	struct filetype **children; // 子节点指针数组
	int num_children;			// 子节点数量
	int num_links;				// 硬链接数
	struct filetype *parent;	// 指向父目录的指针
	char type[20];				// 文件类型（如 "file" 或 "directory"）
	mode_t permissions;			// 文件或目录的权限模式
	uid_t user_id;				// 用户 ID
	gid_t group_id;				// 组 ID
	time_t a_time;				// 最后访问时间
	time_t m_time;				// 最后修改时间
	time_t c_time;				// 最后状态更改时间
	time_t b_time;				// 创建时间
	off_t size;					// 文件或目录的大小
	int datablocks[16];			// 数据块编号数组
	int number;					// 文件或目录的编号
	int blocks;					// 文件占用的数据块数量
} filetype;

superblock spblock;
/*
 * initialize_superblock - 初始化超级块
 *
 * 功能：
 * 1. 初始化文件系统的超级块结构。
 * 2. 将数据块位图和 inode 位图初始化为全 0，表示所有数据块和 inode 均为空闲状态。
 *
 * 参数：
 * - 无。
 *
 * 返回值：
 * - 无。
 *
 * 实现逻辑：
 * 1. 使用 memset 将数据块位图（spblock.data_bitmap）初始化为全 0。
 * 2. 使用 memset 将 inode 位图（spblock.inode_bitmap）初始化为全 0。
 *
 * 示例：
 * 假设文件系统刚创建，调用 initialize_superblock() 后：
 * - spblock.data_bitmap 的所有位均为 0，表示所有数据块空闲。
 * - spblock.inode_bitmap 的所有位均为 0，表示所有 inode 空闲。
 *
 * 注意：
 * - 超级块是文件系统的核心数据结构，必须在文件系统初始化时调用此方法。
 * - 数据块位图和 inode 位图的大小固定为 100 个字节。
 */
void initialize_superblock()
{

	memset(spblock.data_bitmap, '0', 100 * sizeof(char));
	memset(spblock.inode_bitmap, '0', 100 * sizeof(char));
}

filetype *root;

filetype file_array[50];

/*
 * tree_to_array - 将文件树结构序列化为数组
 *
 * 功能：
 * 1. 通过广度优先遍历（BFS）将文件树结构扁平化为数组。
 * 2. 将树中的每个节点按遍历顺序存储到数组中。
 * 3. 使用无效节点（valid = 0）填充空位，确保数组长度固定。
 *
 * 参数：
 * - queue: 用于广度优先遍历的队列，存储待处理的节点。
 * - front: 队列的起始索引，指向当前处理的节点。
 * - rear: 队列的结束索引，指向下一个可插入的位置。
 * - index: 当前数组的索引，指向下一个可存储的位置。
 *
 * 实现逻辑：
 * 1. 从队列中取出当前节点（queue[*front]），并将其存储到数组（file_array[*index]）中。
 * 2. 如果当前节点有效（valid = 1），将其子节点加入队列。
 * 3. 如果当前节点无效或子节点不足 5 个，用无效节点填充队列。
 * 4. 递归处理队列中的下一个节点，直到队列为空或数组已满。
 *
 * 示例：
 * 假设文件树结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 序列化后的数组布局如下：
 * Filetype 0 (Root):
 * - valid: 1
 * - path: "/"
 * - name: "/"
 * - type: "directory"
 * - children: [Filetype 1, Filetype 2]
 *
 * Filetype 1 (home):
 * - valid: 1
 * - path: "/home"
 * - name: "home"
 * - type: "directory"
 * - children: [Filetype 3]
 *
 * Filetype 2 (test.txt):
 * - valid: 1
 * - path: "/test.txt"
 * - name: "test.txt"
 * - type: "file"
 * - children: []
 *
 * Filetype 3 (user):
 * - valid: 1
 * - path: "/home/user"
 * - name: "user"
 * - type: "directory"
 * - children: []
 *
 * Filetype 4-30:
 * - valid: 0 (无效节点，用于占位)
 *
 * 注意：
 * - 数组长度固定为 31，包括无效节点。
 * - 每个节点最多有 5 个子节点。
 */
void tree_to_array(filetype *queue, int *front, int *rear, int *index)
{

	if (rear < front)
		return;
	if (*index > 30)
		return;

	filetype curr_node = queue[*front];
	*front += 1;
	file_array[*index] = curr_node;
	*index += 1;

	if (*index < 6)
	{

		if (curr_node.valid)
		{
			int n = 0;
			int i;
			for (i = 0; i < curr_node.num_children; i++)
			{
				if (*rear < *front)
					*rear = *front;
				queue[*rear] = *(curr_node.children[i]);
				*rear += 1;
			}
			while (i < 5)
			{
				filetype waste_node;
				waste_node.valid = 0;
				queue[*rear] = waste_node;
				*rear += 1;
				i++;
			}
		}
		else
		{
			int i = 0;
			while (i < 5)
			{
				filetype waste_node;
				waste_node.valid = 0;
				queue[*rear] = waste_node;
				*rear += 1;
				i++;
			}
		}
	}

	tree_to_array(queue, front, rear, index);
}

/*
 * save_contents - 保存文件系统内容到磁盘
 *
 * 功能：
 * 1. 将内存中的文件树结构通过广度优先遍历序列化为数组。
 * 2. 将序列化后的数组和超级块分别保存到 `file_structure.bin` 和 `super.bin` 文件中。
 *
 * 文件布局：
 * - `file_structure.bin` 存储文件树的所有节点信息，按广度优先遍历顺序扁平化为数组。
 *   - 每个节点包含文件/目录的元数据，如路径、名称、类型、权限、时间戳等。
 *   - 无效节点（valid = 0）用于占位，确保数组长度固定。
 *   - 在内存中，通过 children 指针重建树结构。
 * - `super.bin` 存储超级块信息，包括数据块和 inode 的位图。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * `file_structure.bin` 的布局可能如下：
 * Filetype 0 (Root):
 * - valid: 1
 * - path: "/"
 * - name: "/"
 * - type: "directory"
 * - children: [Filetype 1, Filetype 2]
 *
 * Filetype 1 (home):
 * - valid: 1
 * - path: "/home"
 * - name: "home"
 * - type: "directory"
 * - children: [Filetype 3]
 *
 * Filetype 2 (test.txt):
 * - valid: 1
 * - path: "/test.txt"
 * - name: "test.txt"
 * - type: "file"
 * - children: []
 *
 * Filetype 3 (user):
 * - valid: 1
 * - path: "/home/user"
 * - name: "user"
 * - type: "directory"
 * - children: []
 *
 * Filetype 4-30:
 * - valid: 0 (无效节点，用于占位)
 *
 * 流程：
 * 1. 初始化队列，将根节点加入队列。
 * 2. 通过广度优先遍历将文件树序列化为数组。
 * 3. 将序列化后的数组写入 `file_structure.bin`。
 * 4. 将超级块写入 `super.bin`。
 *
 * 注意：
 * - 文件树最多支持 31 个节点（包括无效节点）。
 * - 每个节点最多有 5 个子节点。
 */
int save_contents()
{
	printf("SAVING\n");
	filetype *queue = malloc(sizeof(filetype) * 60);
	int front = 0;
	int rear = 0;
	queue[0] = *root;
	int index = 0;
	tree_to_array(queue, &front, &rear, &index);

	for (int i = 0; i < 31; i++)
	{
		printf("%d", file_array[i].valid);
	}

	FILE *fd = fopen("file_structure.bin", "wb");

	FILE *fd1 = fopen("super.bin", "wb");

	fwrite(file_array, sizeof(filetype) * 31, 1, fd);
	fwrite(&spblock, sizeof(superblock), 1, fd1);

	fclose(fd);
	fclose(fd1);

	printf("\n");
}

/*
 * initialize_root_directory - 初始化根目录
 *
 * 功能：
 * 1. 创建并初始化文件系统的根目录。
 * 2. 设置根目录的元数据，包括路径、名称、类型、权限、时间戳等。
 * 3. 标记根目录的 inode 为已使用。
 * 4. 调用 save_contents 方法将初始化后的文件系统保存到磁盘。
 *
 * 实现逻辑：
 * 1. 在 inode 位图中标记根目录的 inode 为已使用（spblock.inode_bitmap[1] = 1）。
 * 2. 分配内存并初始化根目录结构（filetype）。
 * 3. 设置根目录的路径为 "/"，名称为 "/"。
 * 4. 设置根目录的类型为 "directory"，权限为 0777。
 * 5. 初始化时间戳（创建时间、访问时间、修改时间等）。
 * 6. 设置根目录的 inode 编号为 2。
 * 7. 调用 save_contents 方法保存文件系统。
 *
 * 初始化后的根目录结构示例：
 * Filetype (Root):
 * - valid: 1
 * - path: "/"
 * - name: "/"
 * - type: "directory"
 * - permissions: 0777
 * - user_id: 当前用户ID
 * - group_id: 当前组ID
 * - a_time: 当前时间
 * - m_time: 当前时间
 * - c_time: 当前时间
 * - b_time: 当前时间
 * - size: 0
 * - number: 2 (inode 编号)
 * - blocks: 0
 * - children: NULL
 * - num_children: 0
 * - parent: NULL
 * - num_links: 2
 *
 * 注意：
 * - 根目录的 inode 编号固定为 2。
 * - 根目录是文件系统的起点，所有其他文件和目录都是其子节点。
 */
void initialize_root_directory()
{

	spblock.inode_bitmap[1] = 1; // marking it with 0
	root = (filetype *)malloc(sizeof(filetype));

	strcpy(root->path, "/");
	strcpy(root->name, "/");

	root->children = NULL;
	root->num_children = 0;
	root->parent = NULL;
	root->num_links = 2;
	root->valid = 1;
	strcpy(root->test, "test");
	// root -> type = malloc(10);
	strcpy(root->type, "directory");

	root->c_time = time(NULL);
	root->a_time = time(NULL);
	root->m_time = time(NULL);
	root->b_time = time(NULL);

	root->permissions = S_IFDIR | 0777;

	root->size = 0;
	root->group_id = getgid();
	root->user_id = getuid();

	root->number = 2;
	// root -> size = 0;
	root->blocks = 0;

	save_contents();
}

/*
 * filetype_from_path - 根据路径查找对应的文件节点
 *
 * 功能：
 * 1. 根据给定的路径，在文件树中查找对应的文件或目录节点。
 * 2. 支持绝对路径（以 "/" 开头）的解析。
 * 3. 如果路径不存在，返回 NULL。
 *
 * 参数：
 * - path: 要查找的路径字符串（必须以 "/" 开头）。
 *
 * 返回值：
 * - 成功时返回对应的文件节点指针（filetype *）。
 * - 如果路径不存在或路径格式错误，返回 NULL。
 *
 * 实现逻辑：
 * 1. 检查路径是否以 "/" 开头，如果不是则报错并退出。
 * 2. 从根节点开始，逐级解析路径中的目录名。
 * 3. 在当前节点的子节点中查找匹配的目录或文件。
 * 4. 如果找到匹配的节点，返回该节点；否则返回 NULL。
 *
 * 示例：
 * 假设文件树结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 调用示例：
 * 1. filetype_from_path("/") -> 返回根节点
 * 2. filetype_from_path("/home") -> 返回 home 目录节点
 * 3. filetype_from_path("/home/user") -> 返回 user 目录节点
 * 4. filetype_from_path("/test.txt") -> 返回 test.txt 文件节点
 * 5. filetype_from_path("/invalid") -> 返回 NULL
 *
 * 注意：
 * - 路径必须以 "/" 开头。
 * - 路径末尾的 "/" 会被自动去除。
 * - 如果路径不存在，返回 NULL。
 */
filetype *filetype_from_path(char *path)
{
	char curr_folder[100];
	char *path_name = malloc(strlen(path) + 2);

	strcpy(path_name, path);

	filetype *curr_node = root;

	fflush(stdin);

	if (strcmp(path_name, "/") == 0)
		return curr_node;

	if (path_name[0] != '/')
	{
		printf("INCORRECT PATH\n");
		exit(1);
	}
	else
	{
		path_name++;
	}

	if (path_name[strlen(path_name) - 1] == '/')
	{
		path_name[strlen(path_name) - 1] = '\0';
	}

	char *index;
	int flag = 0;

	while (strlen(path_name) != 0)
	{
		index = strchr(path_name, '/');

		if (index != NULL)
		{
			strncpy(curr_folder, path_name, index - path_name);
			curr_folder[index - path_name] = '\0';

			flag = 0;
			for (int i = 0; i < curr_node->num_children; i++)
			{
				if (strcmp((curr_node->children)[i]->name, curr_folder) == 0)
				{
					curr_node = (curr_node->children)[i];
					flag = 1;
					break;
				}
			}
			if (flag == 0)
				return NULL;
		}
		else
		{
			strcpy(curr_folder, path_name);
			flag = 0;
			for (int i = 0; i < curr_node->num_children; i++)
			{
				if (strcmp((curr_node->children)[i]->name, curr_folder) == 0)
				{
					curr_node = (curr_node->children)[i];
					return curr_node;
				}
			}
			return NULL;
		}
		path_name = index + 1;
	}
}
/*
 * find_free_inode - 查找空闲的 inode
 *
 * 功能：
 * 1. 查找当前文件系统中空闲的 inode 编号。
 * 2. 用于创建新文件或目录时分配 inode。
 *
 * 参数：
 * - 无。
 *
 * 返回值：
 * - 成功时返回空闲的 inode 编号。
 * - 如果没有空闲 inode，返回 -1。
 *
 * 实现逻辑：
 * 1. 遍历 inode 表，查找第一个空闲的 inode。
 * 2. 返回找到的 inode 编号。
 *
 * 注意：
 * - 如果 inode 表已满，需扩展文件系统。
 */
int find_free_inode()
{
	for (int i = 2; i < 100; i++)
	{
		if (spblock.inode_bitmap[i] == '0')
		{
			spblock.inode_bitmap[i] = '1';
		}
		return i;
	}
}
/*
 * find_free_db - 查找空闲的数据块
 *
 * 功能：
 * 1. 查找当前文件系统中空闲的数据块编号。
 * 2. 用于存储文件数据时分配数据块。
 *
 * 参数：
 * - 无。
 *
 * 返回值：
 * - 成功时返回空闲的数据块编号。
 * - 如果没有空闲数据块，返回 -1。
 *
 * 实现逻辑：
 * 1. 遍历数据块位图，查找第一个空闲的数据块。
 * 2. 返回找到的数据块编号。
 *
 * 注意：
 * - 如果数据块已满，需扩展文件系统。
 */
int find_free_db()
{
	for (int i = 1; i < 100; i++)
	{
		if (spblock.inode_bitmap[i] == '0')
		{
			spblock.inode_bitmap[i] = '1';
		}
		return i;
	}
}

/*
 * add_child - 添加子节点
 *
 * 功能：
 * 1. 将指定节点添加到父目录的子节点列表中。
 * 2. 更新父目录的子节点数量和列表。
 *
 * 参数：
 * - parent: 父目录节点。
 * - child: 要添加的子节点。
 *
 * 返回值：
 * - 无。
 *
 * 实现逻辑：
 * 1. 扩展父目录的子节点列表（如果需要）。
 * 2. 将子节点添加到列表中。
 * 3. 更新父目录的子节点数量。
 *
 * 注意：
 * - 确保父目录是目录类型。
 */
void add_child(filetype *parent, filetype *child)
{
	(parent->num_children)++;

	parent->children = realloc(parent->children, (parent->num_children) * sizeof(filetype *));

	(parent->children)[parent->num_children - 1] = child;
}

/*
 * mymkdir - 创建新目录
 *
 * 功能：
 * 1. 在指定路径下创建一个新目录。
 * 2. 分配并初始化新目录的元数据，包括路径、名称、类型、权限、时间戳等。
 * 3. 将新目录添加到父目录的子节点列表中。
 * 4. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - path: 新目录的完整路径（必须以 "/" 开头）。
 * - mode: 目录的权限模式（未直接使用，固定为 0777）。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果父目录不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 查找一个空闲的 inode 编号。
 * 2. 解析路径，获取新目录的名称和父目录路径。
 * 3. 分配内存并初始化新目录结构（filetype）。
 * 4. 设置新目录的元数据，包括路径、名称、类型、权限、时间戳等。
 * 5. 将新目录添加到父目录的子节点列表中。
 * 6. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * └── home
 *
 * 调用示例：
 * 1. mymkdir("/home/user", 0777) -> 在 /home 下创建 user 目录
 *    - 新目录路径: "/home/user"
 *    - 新目录名称: "user"
 *    - 父目录: "/home"
 * 2. mymkdir("/invalid/path", 0777) -> 返回 -ENOENT（父目录不存在）
 *
 * 注意：
 * - 路径必须以 "/" 开头。
 * - 新目录的权限固定为 0777。
 * - 如果父目录不存在，操作失败。
 */
static int mymkdir(const char *path, mode_t mode)
{
	printf("MKDIR\n");

	int index = find_free_inode();

	filetype *new_folder = malloc(sizeof(filetype));

	char *pathname = malloc(strlen(path) + 2);
	strcpy(pathname, path);

	char *rindex = strrchr(pathname, '/');

	// new_folder -> name = malloc(strlen(pathname)+2);
	strcpy(new_folder->name, rindex + 1);
	// new_folder -> path = malloc(strlen(pathname)+2);
	strcpy(new_folder->path, pathname);

	*rindex = '\0';

	if (strlen(pathname) == 0)
		strcpy(pathname, "/");

	new_folder->children = NULL;
	new_folder->num_children = 0;
	new_folder->parent = filetype_from_path(pathname);
	new_folder->num_links = 2;
	new_folder->valid = 1;
	strcpy(new_folder->test, "test");

	if (new_folder->parent == NULL)
		return -ENOENT;

	// printf(";;;;%p;;;;\n", new_folder);

	add_child(new_folder->parent, new_folder);

	// new_folder -> type = malloc(10);
	strcpy(new_folder->type, "directory");

	new_folder->c_time = time(NULL);
	new_folder->a_time = time(NULL);
	new_folder->m_time = time(NULL);
	new_folder->b_time = time(NULL);

	new_folder->permissions = S_IFDIR | 0777;

	new_folder->size = 0;
	new_folder->group_id = getgid();
	new_folder->user_id = getuid();

	new_folder->number = index;
	new_folder->blocks = 0;

	save_contents();

	return 0;
}

/*
 * myreaddir - 读取目录内容
 *
 * 功能：
 * 1. 读取指定目录下的所有文件和子目录。
 * 2. 使用 FUSE 提供的 filler 函数将目录项添加到缓冲区。
 * 3. 更新目录的访问时间。
 *
 * 参数：
 * - path: 要读取的目录路径。
 * - buffer: 用于存储目录项的缓冲区。
 * - filler: FUSE 提供的回调函数，用于将目录项添加到缓冲区。
 * - offset: 读取偏移量（未使用）。
 * - fi: 文件信息结构（未使用）。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果目录不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 使用 filler 函数添加 "." 和 ".." 目录项。
 * 2. 根据路径查找对应的目录节点。
 * 3. 如果目录存在，遍历其子节点，使用 filler 函数将每个子节点添加到缓冲区。
 * 4. 更新目录的访问时间。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 调用示例：
 * 1. myreaddir("/", buffer, filler, 0, fi) -> 缓冲区包含 ["home", "test.txt"]
 * 2. myreaddir("/home", buffer, filler, 0, fi) -> 缓冲区包含 ["user"]
 * 3. myreaddir("/invalid", buffer, filler, 0, fi) -> 返回 -ENOENT
 *
 * 注意：
 * - 每次读取目录时，都会更新目录的访问时间。
 * - 如果目录不存在，返回 -ENOENT。
 */
int myreaddir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf("READDIR\n");

	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	char *pathname = malloc(strlen(path) + 2);
	strcpy(pathname, path);

	filetype *dir_node = filetype_from_path(pathname);

	if (dir_node == NULL)
	{
		return -ENOENT;
	}
	else
	{
		dir_node->a_time = time(NULL);
		for (int i = 0; i < dir_node->num_children; i++)
		{
			printf(":%s:\n", dir_node->children[i]->name);
			filler(buffer, dir_node->children[i]->name, NULL, 0);
		}
	}

	return 0;
}

/*
 * mygetattr - 获取文件或目录属性
 *
 * 功能：
 * 1. 获取指定路径的文件或目录的属性信息。
 * 2. 将属性信息填充到 stat 结构中。
 * 3. 支持文件和目录的属性查询。
 *
 * 参数：
 * - path: 要查询的文件或目录的完整路径。
 * - statit: 用于存储属性信息的 stat 结构指针。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果文件或目录不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 根据路径查找对应的文件或目录节点。
 * 2. 如果节点存在，将属性信息填充到 stat 结构中，包括：
 *    - 用户 ID (st_uid)
 *    - 组 ID (st_gid)
 *    - 访问时间 (st_atime)
 *    - 修改时间 (st_mtime)
 *    - 创建时间 (st_ctime)
 *    - 权限模式 (st_mode)
 *    - 硬链接数 (st_nlink)
 *    - 文件大小 (st_size)
 *    - 数据块数 (st_blocks)
 * 3. 如果节点不存在，返回错误码。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 调用示例：
 * 1. mygetattr("/test.txt", statit) -> 获取 test.txt 文件的属性
 * 2. mygetattr("/home", statit) -> 获取 home 目录的属性
 * 3. mygetattr("/invalid", statit) -> 返回 -ENOENT（文件或目录不存在）
 *
 * 注意：
 * - 必须正确处理文件和目录的属性查询。
 */
static int mygetattr(const char *path, struct stat *statit)
{
	char *pathname;
	pathname = (char *)malloc(strlen(path) + 2);

	strcpy(pathname, path);

	printf("GETATTR %s\n", pathname);

	filetype *file_node = filetype_from_path(pathname);
	if (file_node == NULL)
		return -ENOENT;

	statit->st_uid = file_node->user_id;  // The owner of the file/directory is the user who mounted the filesystem
	statit->st_gid = file_node->group_id; // The group of the file/directory is the same as the group of the user who mounted the filesystem
	statit->st_atime = file_node->a_time; // The last "a"ccess of the file/directory is right now
	statit->st_mtime = file_node->m_time; // The last "m"odification of the file/directory is right now
	statit->st_ctime = file_node->c_time;
	statit->st_mode = file_node->permissions;
	statit->st_nlink = file_node->num_links + file_node->num_children;
	statit->st_size = file_node->size;
	statit->st_blocks = file_node->blocks;

	return 0;
}
/*
 * myrmdir - 删除目录
 *
 * 功能：
 * 1. 删除指定路径的目录。
 * 2. 从父目录的子节点列表中移除该目录。
 * 3. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - path: 要删除的目录的完整路径。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果目录不存在，返回 -ENOENT。
 * - 如果目录非空，返回 -ENOTEMPTY。
 *
 * 实现逻辑：
 * 1. 解析路径，获取目录名称和父目录路径。
 * 2. 在父目录的子节点列表中查找匹配的目录。
 * 3. 如果找到匹配的目录且为空，将其从子节点列表中移除。
 * 4. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test
 *
 * 调用示例：
 * 1. myrmdir("/test") -> 删除 test 目录
 * 2. myrmdir("/home/user") -> 返回 -ENOTEMPTY（user 目录非空）
 * 3. myrmdir("/invalid") -> 返回 -ENOENT（目录不存在）
 *
 * 注意：
 * - 只能删除空目录。
 * - 如果目录非空，操作失败。
 */
int myrmdir(const char *path)
{

	char *pathname = malloc(strlen(path) + 2);
	strcpy(pathname, path);

	char *rindex = strrchr(pathname, '/');

	char *folder_delete = malloc(strlen(rindex + 1) + 2);

	strcpy(folder_delete, rindex + 1);

	*rindex = '\0';

	if (strlen(pathname) == 0)
		strcpy(pathname, "/");

	filetype *parent = filetype_from_path(pathname);

	if (parent == NULL)
		return -ENOENT;

	if (parent->num_children == 0)
		return -ENOENT;

	filetype *curr_child = (parent->children)[0];
	int index = 0;
	while (index < (parent->num_children))
	{
		if (strcmp(curr_child->name, folder_delete) == 0)
		{
			break;
		}
		index++;
		curr_child = (parent->children)[index];
	}

	if (index < (parent->num_children))
	{
		if (((parent->children)[index]->num_children) != 0)
			return -ENOTEMPTY;
		for (int i = index + 1; i < (parent->num_children); i++)
		{
			(parent->children)[i - 1] = (parent->children)[i];
		}
		(parent->num_children) -= 1;
	}

	else
	{
		return -ENOENT;
	}

	save_contents();

	return 0;
}
/*
 * myrm - 删除文件
 *
 * 功能：
 * 1. 删除指定路径的文件。
 * 2. 从父目录的子节点列表中移除该文件。
 * 3. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - path: 要删除的文件的完整路径。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果文件不存在，返回 -ENOENT。
 * - 如果文件是目录且非空，返回 -ENOTEMPTY。
 *
 * 实现逻辑：
 * 1. 解析路径，获取文件名和父目录路径。
 * 2. 在父目录的子节点列表中查找匹配的文件。
 * 3. 如果找到匹配的文件，将其从子节点列表中移除。
 * 4. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 调用示例：
 * 1. myrm("/test.txt") -> 删除 test.txt 文件
 * 2. myrm("/home/user") -> 返回 -ENOTEMPTY（user 是目录且非空）
 * 3. myrm("/invalid") -> 返回 -ENOENT（文件不存在）
 *
 * 注意：
 * - 只能删除文件，不能删除目录。
 * - 如果文件是目录且非空，操作失败。
 */
int myrm(const char *path)
{

	char *pathname = malloc(strlen(path) + 2);
	strcpy(pathname, path);

	char *rindex = strrchr(pathname, '/');

	char *folder_delete = malloc(strlen(rindex + 1) + 2);

	strcpy(folder_delete, rindex + 1);

	*rindex = '\0';

	if (strlen(pathname) == 0)
		strcpy(pathname, "/");

	filetype *parent = filetype_from_path(pathname);

	if (parent == NULL)
		return -ENOENT;

	if (parent->num_children == 0)
		return -ENOENT;

	filetype *curr_child = (parent->children)[0];
	int index = 0;
	while (index < (parent->num_children))
	{
		if (strcmp(curr_child->name, folder_delete) == 0)
		{
			break;
		}
		index++;
		curr_child = (parent->children)[index];
	}

	if (index < (parent->num_children))
	{
		if (((parent->children)[index]->num_children) != 0)
			return -ENOTEMPTY;
		for (int i = index + 1; i < (parent->num_children); i++)
		{
			(parent->children)[i - 1] = (parent->children)[i];
		}
		(parent->num_children) -= 1;
	}

	else
	{
		return -ENOENT;
	}

	save_contents();

	return 0;
}

/*
 * mycreate - 创建新文件
 *
 * 功能：
 * 1. 在指定路径下创建一个新文件。
 * 2. 分配并初始化新文件的元数据，包括路径、名称、类型、权限、时间戳等。
 * 3. 将新文件添加到父目录的子节点列表中。
 * 4. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - path: 新文件的完整路径（必须以 "/" 开头）。
 * - mode: 文件的权限模式（未直接使用，固定为 0777）。
 * - fi: 文件信息结构（未使用）。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果父目录不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 查找一个空闲的 inode 编号。
 * 2. 解析路径，获取新文件的名称和父目录路径。
 * 3. 分配内存并初始化新文件结构（filetype）。
 * 4. 设置新文件的元数据，包括路径、名称、类型、权限、时间戳等。
 * 5. 将新文件添加到父目录的子节点列表中。
 * 6. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * └── home
 *
 * 调用示例：
 * 1. mycreate("/home/test.txt", 0777, fi) -> 在 /home 下创建 test.txt 文件
 *    - 新文件路径: "/home/test.txt"
 *    - 新文件名称: "test.txt"
 *    - 父目录: "/home"
 * 2. mycreate("/invalid/path/file.txt", 0777, fi) -> 返回 -ENOENT（父目录不存在）
 *
 * 注意：
 * - 路径必须以 "/" 开头。
 * - 新文件的权限固定为 0777。
 * - 如果父目录不存在，操作失败。
 */
int mycreate(const char *path, mode_t mode, struct fuse_file_info *fi)
{

	printf("CREATEFILE\n");

	int index = find_free_inode();

	filetype *new_file = malloc(sizeof(filetype));

	char *pathname = malloc(strlen(path) + 2);
	strcpy(pathname, path);

	char *rindex = strrchr(pathname, '/');

	strcpy(new_file->name, rindex + 1);
	strcpy(new_file->path, pathname);

	*rindex = '\0';

	if (strlen(pathname) == 0)
		strcpy(pathname, "/");

	new_file->children = NULL;
	new_file->num_children = 0;
	new_file->parent = filetype_from_path(pathname);
	new_file->num_links = 0;
	new_file->valid = 1;

	if (new_file->parent == NULL)
		return -ENOENT;

	add_child(new_file->parent, new_file);

	// new_file -> type = malloc(10);
	strcpy(new_file->type, "file");

	new_file->c_time = time(NULL);
	new_file->a_time = time(NULL);
	new_file->m_time = time(NULL);
	new_file->b_time = time(NULL);

	new_file->permissions = S_IFREG | 0777;

	new_file->size = 0;
	new_file->group_id = getgid();
	new_file->user_id = getuid();

	new_file->number = index;

	for (int i = 0; i < 16; i++)
	{
		(new_file->datablocks)[i] = find_free_db();
	}

	// new_file -> size = 0;
	new_file->blocks = 0;

	save_contents();

	return 0;
}

/*
 * myopen - 打开文件
 *
 * 功能：
 * 1. 打开指定路径的文件。
 * 2. 检查文件是否存在。
 * 3. 更新文件的访问时间。
 *
 * 参数：
 * - path: 要打开的文件的完整路径。
 * - fi: 文件信息结构（未使用）。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果文件不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 根据路径查找对应的文件节点。
 * 2. 如果文件存在，更新文件的访问时间。
 * 3. 如果文件不存在，返回错误码。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * └── test.txt
 *
 * 调用示例：
 * 1. myopen("/test.txt", fi) -> 成功打开 test.txt 文件
 * 2. myopen("/invalid.txt", fi) -> 返回 -ENOENT（文件不存在）
 *
 * 注意：
 * - 打开文件时会更新文件的访问时间。
 * - 如果文件不存在，返回 -ENOENT。
 */
int myopen(const char *path, struct fuse_file_info *fi)
{
	printf("OPEN\n");

	char *pathname = malloc(sizeof(path) + 1);
	strcpy(pathname, path);

	filetype *file = filetype_from_path(pathname);

	return 0;
}

/*
 * myread - 读取文件内容
 *
 * 功能：
 * 1. 读取指定文件的内容。
 * 2. 根据文件的大小和数据块分布，从磁盘中读取数据。
 * 3. 将读取的数据存储到提供的缓冲区中。
 * 4. 更新文件的访问时间。
 *
 * 参数：
 * - path: 要读取的文件的完整路径。
 * - buf: 用于存储读取数据的缓冲区。
 * - size: 要读取的数据大小。
 * - offset: 读取的偏移量（未使用）。
 * - fi: 文件信息结构（未使用）。
 *
 * 返回值：
 * - 成功时返回实际读取的字节数。
 * - 如果文件不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 根据路径查找对应的文件节点。
 * 2. 如果文件存在，根据文件的大小和数据块分布，从磁盘中读取数据。
 * 3. 将读取的数据存储到提供的缓冲区中。
 * 4. 更新文件的访问时间。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * └── test.txt
 *
 * 调用示例：
 * 1. myread("/test.txt", buf, 1024, 0, fi) -> 读取 test.txt 文件的前 1024 字节
 * 2. myread("/invalid.txt", buf, 1024, 0, fi) -> 返回 -ENOENT（文件不存在）
 *
 * 注意：
 * - 读取数据时会更新文件的访问时间。
 * - 如果文件不存在，返回 -ENOENT。
 */
int myread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{

	printf("READ\n");

	char *pathname = malloc(sizeof(path) + 1);
	strcpy(pathname, path);

	filetype *file = filetype_from_path(pathname);
	if (file == NULL)
		return -ENOENT;

	else
	{
		char *str = malloc(sizeof(char) * 1024 * (file->blocks));

		printf(":%ld:\n", file->size);
		strcpy(str, "");
		int i;
		for (i = 0; i < (file->blocks) - 1; i++)
		{
			strncat(str, &spblock.datablocks[block_size * (file->datablocks[i])], 1024);
			printf("--> %s", str);
		}
		strncat(str, &spblock.datablocks[block_size * (file->datablocks[i])], (file->size) % 1024);
		printf("--> %s", str);
		// strncpy(str, &spblock.datablocks[block_size*(file -> datablocks[0])], file->size);
		strcpy(buf, str);
	}
	return file->size;
}

/*
 * myaccess - 检查文件访问权限
 *
 * 功能：
 * 1. 检查指定路径的文件或目录是否可访问。
 * 2. 目前仅作为占位符实现，始终返回成功。
 *
 * 参数：
 * - path: 要检查的文件或目录的完整路径。
 * - mask: 访问权限掩码（未使用）。
 *
 * 返回值：
 * - 始终返回 0，表示成功。
 *
 * 注意：
 * - 当前实现未实际检查权限，需根据需求完善。
 */
int myaccess(const char *path, int mask)
{
	return 0;
}

/*
 * myrename - 重命名文件或目录
 *
 * 功能：
 * 1. 将文件或目录从旧路径重命名为新路径。
 * 2. 更新文件或目录的名称和路径。
 * 3. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - from: 文件或目录的原始路径。
 * - to: 文件或目录的新路径。
 *
 * 返回值：
 * - 成功时返回 0。
 * - 如果原始路径对应的文件或目录不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 解析原始路径，获取文件或目录的节点。
 * 2. 解析新路径，获取新名称和父目录路径。
 * 3. 更新文件或目录的名称和路径。
 * 4. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * ├── home
 * │   └── user
 * └── test.txt
 *
 * 调用示例：
 * 1. myrename("/test.txt", "/home/test.txt") -> 将 test.txt 移动到 /home 目录下
 * 2. myrename("/home/user", "/home/new_user") -> 将 user 目录重命名为 new_user
 * 3. myrename("/invalid", "/new_path") -> 返回 -ENOENT（文件或目录不存在）
 *
 * 注意：
 * - 重命名操作会同时更新文件或目录的名称和路径。
 * - 如果原始路径对应的文件或目录不存在，返回 -ENOENT。
 */
int myrename(const char *from, const char *to)
{
	printf("RENAME: %s\n", from);
	printf("RENAME: %s\n", to);

	char *pathname = malloc(strlen(from) + 2);
	strcpy(pathname, from);

	char *rindex1 = strrchr(pathname, '/');

	filetype *file = filetype_from_path(pathname);

	*rindex1 = '\0';

	char *pathname2 = malloc(strlen(to) + 2);
	strcpy(pathname2, to);

	char *rindex2 = strrchr(pathname2, '/');

	if (file == NULL)
		return -ENOENT;

	// file -> name = realloc(file -> name, strlen(rindex2+1)+2);
	strcpy(file->name, rindex2 + 1);
	// file -> path = realloc(file -> path, strlen(to)+2);
	strcpy(file->path, to);

	printf(":%s:\n", file->name);
	printf(":%s:\n", file->path);

	save_contents();

	return 0;
}

/*
 * mytruncate - 截断文件
 *
 * 功能：
 * 1. 将指定文件截断到指定大小。
 * 2. 目前仅作为占位符实现，未实际实现功能。
 *
 * 参数：
 * - path: 要截断的文件的完整路径。
 * - size: 目标文件大小。
 *
 * 返回值：
 * - 始终返回 0，表示成功。
 *
 * 注意：
 * - 当前实现未实际截断文件，需根据需求完善。
 */
int mytruncate(const char *path, off_t size)
{
	return 0;
}

/*
 * mywrite - 向文件写入数据
 *
 * 功能：
 * 1. 将数据写入指定文件。
 * 2. 根据文件当前大小和数据块使用情况，将数据写入合适的块。
 * 3. 更新文件的大小和块使用情况。
 * 4. 调用 save_contents 方法将更新后的文件系统保存到磁盘。
 *
 * 参数：
 * - path: 要写入的文件的完整路径。
 * - buf: 要写入的数据缓冲区。
 * - size: 要写入的数据大小。
 * - offset: 写入的偏移量（未使用）。
 * - fi: 文件信息结构（未使用）。
 *
 * 返回值：
 * - 成功时返回实际写入的字节数。
 * - 如果文件不存在，返回 -ENOENT。
 *
 * 实现逻辑：
 * 1. 根据路径查找对应的文件节点。
 * 2. 如果文件当前大小为 0，直接将数据写入第一个数据块。
 * 3. 如果文件已有数据，检查最后一个数据块是否有剩余空间：
 *    - 如果有剩余空间，将数据写入剩余空间。
 *    - 如果没有剩余空间，分配新的数据块并写入数据。
 * 4. 更新文件的大小和块使用情况。
 * 5. 调用 save_contents 方法保存文件系统。
 *
 * 示例：
 * 假设文件系统结构如下：
 * /
 * └── test.txt
 *
 * 调用示例：
 * 1. mywrite("/test.txt", "Hello", 5, 0, fi) -> 将 "Hello" 写入 test.txt 文件
 *    - 文件大小更新为 5
 *    - 数据块 0 包含 "Hello"
 * 2. mywrite("/test.txt", " World", 6, 0, fi) -> 将 " World" 追加到 test.txt 文件
 *    - 文件大小更新为 11
 *    - 数据块 0 包含 "Hello World"
 * 3. mywrite("/invalid.txt", "Data", 4, 0, fi) -> 返回 -ENOENT（文件不存在）
 *
 * 注意：
 * - 写入数据时会自动追加到文件末尾。
 * - 如果文件不存在，返回 -ENOENT。
 */
int mywrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{

	printf("WRITING\n");

	char *pathname = malloc(sizeof(path) + 1);
	strcpy(pathname, path);

	filetype *file = filetype_from_path(pathname);
	if (file == NULL)
		return -ENOENT;

	int indexno = (file->blocks) - 1;

	if (file->size == 0)
	{
		strcpy(&spblock.datablocks[block_size * ((file->datablocks)[0])], buf);
		file->size = strlen(buf);
		(file->blocks)++;
	}
	else
	{
		int currblk = (file->blocks) - 1;
		int len1 = 1024 - (file->size % 1024);
		if (len1 >= strlen(buf))
		{
			strcat(&spblock.datablocks[block_size * ((file->datablocks)[currblk])], buf);
			file->size += strlen(buf);
			printf("---> %s\n", &spblock.datablocks[block_size * ((file->datablocks)[currblk])]);
		}
		else
		{
			char *cpystr = malloc(1024 * sizeof(char));
			strncpy(cpystr, buf, len1 - 1);
			strcat(&spblock.datablocks[block_size * ((file->datablocks)[currblk])], cpystr);
			strcpy(cpystr, buf);
			strcpy(&spblock.datablocks[block_size * ((file->datablocks)[currblk + 1])], (cpystr + len1 - 1));
			file->size += strlen(buf);
			printf("---> %s\n", &spblock.datablocks[block_size * ((file->datablocks)[currblk])]);
			(file->blocks)++;
		}
	}
	save_contents();

	return strlen(buf);
}

static struct fuse_operations operations =
{
    .mkdir = mymkdir,       // 创建目录
    .getattr = mygetattr,   // 获取文件/目录属性
    .readdir = myreaddir,   // 读取目录内容
    .rmdir = myrmdir,       // 删除目录
    .open = myopen,         // 打开文件
    .read = myread,         // 读取文件内容
    .write = mywrite,       // 写入文件内容
    .create = mycreate,     // 创建文件
    .rename = myrename,     // 重命名文件/目录
    .unlink = myrm,         // 删除文件
};

int main(int argc, char *argv[])
{
	// 二进制文件代表了基于磁盘的文件系统（file layout)
	FILE *fd = fopen("file_structure.bin", "rb");
	if (fd)
	{
		printf("LOADING\n");
		// 如果文件存在，读取文件结构数据到 file_array
		fread(&file_array, sizeof(filetype) * 31, 1, fd);

		// 初始化子节点索引
		int child_startindex = 1;
		// 设置根节点的父节点为 NULL
		file_array[0].parent = NULL;

		// 遍历前 6 个节点，在内存中重建文件树结构
		for (int i = 0; i < 6; i++)
		{
			file_array[i].num_children = 0;
			file_array[i].children = NULL;
			// 为每个节点添加子节点
			for (int j = child_startindex; j < child_startindex + 5; j++)
			{
				if (file_array[j].valid)
				{
					add_child(&file_array[i], &file_array[j]);
				}
			}
			child_startindex += 5;
		}

		// 设置根节点
		root = &file_array[0];

		// 打开并读取超级块数据
		FILE *fd1 = fopen("super.bin", "rb");
		fread(&spblock, sizeof(superblock), 1, fd1);
	}
	else
	{
		// 如果文件不存在，初始化超级块和根目录
		initialize_superblock();
		initialize_root_directory();
	}

	// FUSE 库的主入口函数，用于启动文件系统, 指向 fuse_operations 结构体的指针
	return fuse_main(argc, argv, &operations, NULL);
}