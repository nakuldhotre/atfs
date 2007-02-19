/*
 * linux/fs/ext3/ioctl.c
 *
 * Copyright (C) 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 */

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/capability.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#define SUCCESS 0
#define UNALLOC -1
#define NOTFOUND -2
struct atfs_appl atfs_appl_ll;
int find_group(char *,struct super_block *);
void init_appl_ll()
{
	INIT_LIST_HEAD(&atfs_appl_ll.appl_list);
}
/* add_appl() creates  and entry for the application in the db*/
void add_appl(char *name)
{
	struct atfs_appl *temp;
	temp = (struct atfs_appl *)vmalloc(sizeof(struct atfs_appl));  // TODO: find the appropriate kernel space memory alloc function 
	strcpy(temp->appl_name,name);
	printk(KERN_ALERT "ADDING APPLICATION = %s\n",temp->appl_name);
	INIT_LIST_HEAD(&temp->acc_pat1.acc_pat_list);
	list_add_tail(&(temp->appl_list),&atfs_appl_ll.appl_list);
}
/*add_acc_pat() adds a new pattern to the comp 
Note: makes an entry for the pattern the components are not added*/
void add_acc_pat(char appl_name[255],int pat_no,int group_no)
{
	struct atfs_acc_pat *temp;
	struct atfs_appl *appl_temp;
	struct list_head *appl_pos;
	printk(KERN_ALERT "INSIDE add_acc_pat\n");
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos, struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			temp = (struct atfs_acc_pat *)vmalloc(sizeof(struct atfs_acc_pat));
			temp->pat_no = pat_no;
			temp->alloc_group_num = group_no;
			INIT_LIST_HEAD(&temp->comp1.acc_pat_comp_list);
			INIT_LIST_HEAD(&temp->alloc_group_list);
			list_add_tail(&(temp->acc_pat_list),&(appl_temp->acc_pat1.acc_pat_list));
			printk(KERN_ALERT "Adding Access pattern no %d for appl %s and grp no %d\n",pat_no,appl_name,group_no);
			return ;
		}
	}
	//NOT FOUND ROUTINE  ... this case should not occur if we do proper validation at config file parsing time 
}
/* add_comp_to_acc_pat() addes a compenent to the pattern ... pattern is specified by appl_name,pat_no */
void add_comp_to_acc_pat(char appl_name[255],int pat_no,struct atfs_acc_pat_comp comp, struct super_block *sb)
{
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pos,*appl_pat_pos;
	int ret;
	comp_temp = (struct atfs_acc_pat_comp *)vmalloc(sizeof(struct atfs_acc_pat_comp));
	strcpy(comp_temp->path,comp.path);
	comp_temp->estd_size = comp.estd_size;
	comp_temp->type = comp.type;
	
	
	//printk(KERN_ALERT "%s %d %s\n",appl_name,pat_no,comp.path);
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos,struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				if(appl_pat_temp->pat_no==pat_no)
				{
					list_add_tail(&comp_temp->acc_pat_comp_list,&(appl_pat_temp->comp1.acc_pat_comp_list));
					//if(!appl_pat_temp->alloc_group_num)
					//{
					/* check for existence of file get the group number if the file exists store group number in group_alloc_num else store 0*/
					//appl_pat_temp->alloc_group_num=find_group(comp_temp->path,sb);
					//printk(KERN_INFO "Found group number %d for %s\n", appl_pat_temp->alloc_group_num,comp_temp->path);
					//shrink_dcache_sb(sb);
					//}
					if((ret=find_group(comp_temp->path,sb))>=0)
					add_group_num(appl_pat_temp,ret);
					//printk(KERN_ALERT "ADDED %s to %s pat no %d\n",comp.path,appl_name,pat_no);
					return ;
				}
			}
		}
	}
	//NOT FOUND ROUTINE
}

void add_group_num (struct atfs_acc_pat * acc_pat, int group_num)
{
	struct atfs_alloc_group *temp_alloc_group,*new_alloc_group;
	struct list_head *pos_alloc_group;
	
	if(!list_empty(&acc_pat->alloc_group_list))
	list_for_each(pos_alloc_group, &acc_pat->alloc_group_list)
	{
		temp_alloc_group = list_entry(pos_alloc_group,struct atfs_alloc_group,alloc_group_list);
		if(temp_alloc_group->group_no==group_num) return;
	}
	printk("Access pattern didnt have this group..adding %d\n",group_num);
	new_alloc_group = (struct atfs_alloc_group *) vmalloc(sizeof(struct atfs_alloc_group));
	new_alloc_group->group_no = group_num;	
	list_add_tail(&(new_alloc_group->alloc_group_list),&(acc_pat->alloc_group_list));
}



int find_group(char *path, struct super_block *sb)
{
	struct dentry *dentry, *parent;
	char *path_comp;
	struct inode *inode=NULL;
	struct buffer_head *bh;
	struct ext3_dir_entry_2 *de;
	int i,bg;
	
	//show_orphan_list(sb);
	dentry = (struct dentry *) vmalloc(sizeof(struct dentry));
	parent=sb->s_root;
	path_comp=(char *)vmalloc(EXT3_NAME_LEN);
	//inode = sb->s_root->d_inode;
//	printk(KERN_INFO "%s\n", path);
/*	//NAKUL
	dentry = d_alloc_name(parent, path);
	bh = ext3_find_entry(dentry,&de);
	if (bh) 
	{
			unsigned long ino = le32_to_cpu(de->inode);
			brelse (bh);
			inode = iget(sb, ino);
			dput(dentry);
			
	}
	else
	{
		dput(dentry);
		return 2;
	}
	bg = EXT3_I(inode)->i_block_group;
	iput(inode);
	return bg;
	//END NAKUL
	//return 4;
	*/
	while(*path!='\0')
	{
		while(*path=='/') 
		{
		//	printk(KERN_ALERT "-%c-%c- ",*path,*(path+1));
			path++;
		}
		if(*path=='\0') 
			break;
	//	printk(KERN_INFO "--%c--", *path);
		i=0;
		while(*path!='/'&&*path!='\0')
			path_comp[i++]=*path++;
		path_comp[i]='\0';
	//	printk(KERN_ALERT "Searching for %s in %s\n", path_comp, parent->d_name.name);
		dentry = d_alloc_name(parent, path_comp);
		bh = ext3_find_entry(dentry,&de);
		if (bh) 
		{
			unsigned long ino = le32_to_cpu(de->inode);
			brelse (bh);
			inode = iget(sb, ino);
		//	printk(KERN_ALERT "Inode number- %d\n", inode->i_ino);
			if (!inode)
				return -EACCES;
			//atomic_inc(&inode->i_count);
			//d_instantiate(dentry, inode);
			//parent = dentry;			
			parent=d_splice_alias(inode,dentry);
		}
		else
		{
		//	printk(KERN_ALERT "Entry not found\n");
			parent=dentry->d_parent;
			dput(dentry);
			dentry=parent;
			while(dentry!=sb->s_root)
			{
				parent = dentry->d_parent;
			//	printk("Putting inode for %s no %ld\n", dentry->d_name.name,dentry->d_inode->i_ino);
			//	iput(dentry->d_inode);
				dput(dentry);
				dentry = parent;
			}			
			//dump_orphan_list(sb,EXT3_SB(sb));
			//show_orphan_list(sb);	
			return -2;				//return some default
		}
	}
	if(inode)
	{
		bg = EXT3_I(inode)->i_block_group;
		while(dentry!=sb->s_root)
		{
			parent = dentry->d_parent;
		//	printk("Putting inode for %s no %ld\n", dentry->d_name.name,dentry->d_inode->i_ino);
		//	iput(dentry->d_inode);
			dput(dentry);
			dentry = parent;
		}	
		//dump_orphan_list(sb,EXT3_SB(sb));
		//iput(inode);
		//show_orphan_list(sb);
		return bg;
	}
	return -2;
}



void show_orphan_list(struct super_block *sb)
{
	struct list_head *pos_inode;
	struct ext3_inode_info *temp_ie;
	struct inode *inode = sb->s_root->d_inode,*temp_inode;
	
	printk("Orphan list for %ld\n", inode->i_ino);
	if(!list_empty(&(EXT3_I(inode)->i_orphan)))
	{
		list_for_each(pos_inode,&EXT3_I(inode)->i_orphan)
		{
			temp_ie = list_entry(pos_inode,struct ext3_inode_info,i_orphan);
			printk(KERN_ALERT "Inode number - %ld\t", temp_ie->vfs_inode.i_ino);
		}
	}
	printk("All inodes\n");
	if(!list_empty(&sb->s_inodes))
	{
		list_for_each(pos_inode,&sb->s_inodes)
		{
			temp_inode = list_entry(pos_inode,struct inode,i_list);
			printk(KERN_ALERT "Inode number - %ld\t", temp_inode->i_ino);
		}
	}
	printk("Dirty inodes\n");
	if(!list_empty(&sb->s_dirty))
	{
		list_for_each(pos_inode,&sb->s_inodes)
		{
			temp_inode = list_entry(pos_inode,struct inode,i_list);
			printk(KERN_ALERT "Inode number - %ld\t", temp_inode->i_ino);
		}
	}
	
}

/*displays all the access patterns specific to an application i.e appl_name ....   this is just a test and debug function and of no other use */
void display_appl_pats(char appl_name[255])
{
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pat_pos,*appl_pos,*appl_pat_comp_pos;
	printk(KERN_ALERT "Inside display_appl_pat \n");
	if(list_empty(&atfs_appl_ll.appl_list))
	{
		printk(KERN_ALERT "No application info present\n");
	}
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos, struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				printk("Pattern no %d\n",appl_pat_temp->pat_no);
				list_for_each(appl_pat_comp_pos,&(appl_pat_temp->comp1.acc_pat_comp_list))
				{
					comp_temp = list_entry(appl_pat_comp_pos,struct atfs_acc_pat_comp,acc_pat_comp_list);
					printk("%s \n",comp_temp->path);
				}
			}
		}
	}
}
/* find_group_num() returns the assigned group number for component based on the access pattern ... */
struct list_head * find_group_num(char appl_name[255],char *path,int *estd_size)
{
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pos,*appl_pat_pos,*comp_pos;
	int i=0;
	char *dir;
	if(list_empty(&atfs_appl_ll.appl_list))
	{
	//	printk(KERN_ALERT "No application info present\n");
		return NULL;
	}
	i=strlen(path)-1;
	dir=(char *)vmalloc(PATH_MAX);
	while(path[i]!='/'&&i>0) i--;
	memcpy(dir,path,i+1);
	dir[i+1]=0;
	printk("parent path - %s\n",	dir);
	
	//printk(KERN_ALERT "find_group_num():Finding %s %s\n",appl_name,path);
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos,struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				
				list_for_each(comp_pos,&(appl_pat_temp->comp1.acc_pat_comp_list))
				{
					comp_temp = list_entry(comp_pos,struct atfs_acc_pat_comp,acc_pat_comp_list);
					if(comp_temp->type==ATFS_COMP_DIR)
					{
						if(!strcmp(comp_temp->path,dir))
						{
							*estd_size = comp_temp->estd_size;
							return &appl_pat_temp->alloc_group_list;
						}
					}
					else
					{
						if(!strcmp(comp_temp->path,path))
						{
							*estd_size = comp_temp->estd_size;
							return &appl_pat_temp->alloc_group_list; 
						}
					}
				} 
			}
		}
	}
	return NULL;   
}
void set_group_num(char appl_name[255],char *path,int group)
{
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pos,*appl_pat_pos,*comp_pos;
	char *dir;
	int dirlen;
	
	dirlen=strlen(path)-1;
	dir=(char *)vmalloc(PATH_MAX);
	while(path[dirlen]!='/'&&dirlen>0) dirlen--;
	memcpy(dir,path,dirlen+1);
	dir[dirlen+1] = 0;
	//printk("dir = %s\n", dir);
	
	if(list_empty(&atfs_appl_ll.appl_list))
	{
		printk(KERN_ALERT "No application info present\n");
		return;
	}
	//printk(KERN_ALERT "find_group_num():Finding %s %s\n",appl_name,path);
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos,struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				
				list_for_each(comp_pos,&(appl_pat_temp->comp1.acc_pat_comp_list))
				{
					comp_temp = list_entry(comp_pos,struct atfs_acc_pat_comp,acc_pat_comp_list);
					if(comp_temp->type==ATFS_COMP_DIR)
					{
						if(!strcmp(comp_temp->path,dir))
						{
							add_group_num(appl_pat_temp,group);
							return;
						}							
					}
					else
						if(!strcmp(comp_temp->path,path))
							add_group_num(appl_pat_temp,group); 
						return;
					
				} 
			}
		}
	}}

int find_file_estd_size(char appl_name[255],char *path)
{
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pos,*appl_pat_pos,*comp_pos;

	if(list_empty(&atfs_appl_ll.appl_list))
	{
		printk(KERN_ALERT "No application info present\n");
		return -1;
	}
	//printk(KERN_ALERT "file_file_estd_size(): Finding %s %s\n",appl_name,path);
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos,struct atfs_appl, appl_list);
		if(!strcmp(appl_temp->appl_name,appl_name))
		{
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				
				list_for_each(comp_pos,&(appl_pat_temp->comp1.acc_pat_comp_list))
				{
					comp_temp = list_entry(comp_pos,struct atfs_acc_pat_comp,acc_pat_comp_list);
					//printk(KERN_ALERT "comparing with %s\n",comp_temp->path);
					if(!strcmp(comp_temp->path,path))
					{
					//	printk(KERN_ALERT "FOUND THE FILE\n");
						return comp_temp->estd_size; 
					}
				} 
			}
		}
	}
		return NOTFOUND;   
}
struct ext3_our_struct info_array[255];
__u8 info_array_count;

int ext3_ioctl (struct inode * inode, struct file * filp, unsigned int cmd,
		unsigned long arg)
{
	struct ext3_inode_info *ei = EXT3_I(inode);
	unsigned int flags;
	unsigned short rsv_window_size;
	int i,copy_status;
	struct atfs_u_appl *temp,*temp1;
	struct atfs_u_acc_pat *temp2;
	struct atfs_u_acc_pat_comp *temp3;
	struct atfs_acc_pat_comp temp4;
	temp1 = (struct atfs_u_appl *)vmalloc(sizeof(struct atfs_u_appl )); 
	temp2 = (struct atfs_u_acc_pat *)vmalloc(sizeof(struct atfs_u_acc_pat )); 
	temp3 = (struct atfs_u_acc_pat_comp *)vmalloc(sizeof(struct atfs_u_acc_pat_comp ));
	ext3_debug ("cmd = %u, arg = %lu\n", cmd, arg);

	switch (cmd) {
	case EXT3_IOC_GETFLAGS:
		flags = ei->i_flags & EXT3_FL_USER_VISIBLE;
		return put_user(flags, (int __user *) arg);
	case EXT3_IOC_SETFLAGS: {
		handle_t *handle = NULL;
		int err;
		struct ext3_iloc iloc;
		unsigned int oldflags;
		unsigned int jflag;

		if (IS_RDONLY(inode))
			return -EROFS;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EACCES;

		if (get_user(flags, (int __user *) arg))
			return -EFAULT;

		if (!S_ISDIR(inode->i_mode))
			flags &= ~EXT3_DIRSYNC_FL;

		mutex_lock(&inode->i_mutex);
		oldflags = ei->i_flags;

		/* The JOURNAL_DATA flag is modifiable only by root */
		jflag = flags & EXT3_JOURNAL_DATA_FL;

		/*
		 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
		 * the relevant capability.
		 *
		 * This test looks nicer. Thanks to Pauline Middelink
		 */
		if ((flags ^ oldflags) & (EXT3_APPEND_FL | EXT3_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				mutex_unlock(&inode->i_mutex);
				return -EPERM;
			}
		}

		/*
		 * The JOURNAL_DATA flag can only be changed by
		 * the relevant capability.
		 */
		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL)) {
			if (!capable(CAP_SYS_RESOURCE)) {
				mutex_unlock(&inode->i_mutex);
				return -EPERM;
			}
		}


		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle)) {
			mutex_unlock(&inode->i_mutex);
			return PTR_ERR(handle);
		}
		if (IS_SYNC(inode))
			handle->h_sync = 1;
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err)
			goto flags_err;

		flags = flags & EXT3_FL_USER_MODIFIABLE;
		flags |= oldflags & ~EXT3_FL_USER_MODIFIABLE;
		ei->i_flags = flags;

		ext3_set_inode_flags(inode);
		inode->i_ctime = CURRENT_TIME_SEC;

		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
flags_err:
		ext3_journal_stop(handle);
		if (err) {
			mutex_unlock(&inode->i_mutex);
			return err;
		}

		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL))
			err = ext3_change_inode_journal_flag(inode, jflag);
		mutex_unlock(&inode->i_mutex);
		return err;
	}
	case EXT3_IOC_GETVERSION:
	case EXT3_IOC_GETVERSION_OLD:
		return put_user(inode->i_generation, (int __user *) arg);
	case EXT3_IOC_SETVERSION:
	case EXT3_IOC_SETVERSION_OLD: {
		handle_t *handle;
		struct ext3_iloc iloc;
		__u32 generation;
		int err;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EPERM;
		if (IS_RDONLY(inode))
			return -EROFS;
		if (get_user(generation, (int __user *) arg))
			return -EFAULT;

		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err == 0) {
			inode->i_ctime = CURRENT_TIME_SEC;
			inode->i_generation = generation;
			err = ext3_mark_iloc_dirty(handle, inode, &iloc);
		}
		ext3_journal_stop(handle);
		return err;
	}
#ifdef CONFIG_JBD_DEBUG
	case EXT3_IOC_WAIT_FOR_READONLY:
		/*
		 * This is racy - by the time we're woken up and running,
		 * the superblock could be released.  And the module could
		 * have been unloaded.  So sue me.
		 *
		 * Returns 1 if it slept, else zero.
		 */
		{
			struct super_block *sb = inode->i_sb;
			DECLARE_WAITQUEUE(wait, current);
			int ret = 0;

			set_current_state(TASK_INTERRUPTIBLE);
			add_wait_queue(&EXT3_SB(sb)->ro_wait_queue, &wait);
			if (timer_pending(&EXT3_SB(sb)->turn_ro_timer)) {
				schedule();
				ret = 1;
			}
			remove_wait_queue(&EXT3_SB(sb)->ro_wait_queue, &wait);
			return ret;
		}
#endif
	case EXT3_IOC_GETRSVSZ:
		if (test_opt(inode->i_sb, RESERVATION)
			&& S_ISREG(inode->i_mode)
			&& ei->i_block_alloc_info) {
			rsv_window_size = ei->i_block_alloc_info->rsv_window_node.rsv_goal_size;
			return put_user(rsv_window_size, (int __user *)arg);
		}
		return -ENOTTY;
	case EXT3_IOC_SETRSVSZ: {

		if (!test_opt(inode->i_sb, RESERVATION) ||!S_ISREG(inode->i_mode))
			return -ENOTTY;

		if (IS_RDONLY(inode))
			return -EROFS;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EACCES;

		if (get_user(rsv_window_size, (int __user *)arg))
			return -EFAULT;

		if (rsv_window_size > EXT3_MAX_RESERVE_BLOCKS)
			rsv_window_size = EXT3_MAX_RESERVE_BLOCKS;

		/*
		 * need to allocate reservation structure for this inode
		 * before set the window size
		 */
		mutex_lock(&ei->truncate_mutex);
		if (!ei->i_block_alloc_info)
			ext3_init_block_alloc_info(inode);

		if (ei->i_block_alloc_info){
			struct ext3_reserve_window_node *rsv = &ei->i_block_alloc_info->rsv_window_node;
			rsv->rsv_goal_size = rsv_window_size;
		}
		mutex_unlock(&ei->truncate_mutex);
		return 0;
	}
	case EXT3_IOC_GROUP_EXTEND: {
		unsigned long n_blocks_count;
		struct super_block *sb = inode->i_sb;
		int err;

		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		if (IS_RDONLY(inode))
			return -EROFS;

		if (get_user(n_blocks_count, (__u32 __user *)arg))
			return -EFAULT;

		err = ext3_group_extend(sb, EXT3_SB(sb)->s_es, n_blocks_count);
		journal_lock_updates(EXT3_SB(sb)->s_journal);
		journal_flush(EXT3_SB(sb)->s_journal);
		journal_unlock_updates(EXT3_SB(sb)->s_journal);

		return err;
	}
	case EXT3_IOC_GROUP_ADD: {
		struct ext3_new_group_data input;
		struct super_block *sb = inode->i_sb;
		int err;

		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		if (IS_RDONLY(inode))
			return -EROFS;

		if (copy_from_user(&input, (struct ext3_new_group_input __user *)arg,
				sizeof(input)))
			return -EFAULT;

		err = ext3_group_add(sb, &input);
		journal_lock_updates(EXT3_SB(sb)->s_journal);
		journal_flush(EXT3_SB(sb)->s_journal);
		journal_unlock_updates(EXT3_SB(sb)->s_journal);

		return err;
	}
	case EXT3_IOC_PASSINFO: {
		struct ext3_our_struct input;
		if(copy_from_user(&input,(struct ext3_our_struct __user *)arg,sizeof(input)))
		{
			printk("returning -efault");
			return -EFAULT;
		}
		printk("<1> Got info via IOCTL \n");
		printk("<1> name = %s\n",input.name);
		printk("<1> size = %u\n",input.size);
		strcpy(info_array[info_array_count].name,input.name);
		info_array[info_array_count].type = input.type;
		info_array[info_array_count].size = input.size;
		info_array_count++;
		return 0;
	case EXT3_IOC_ADDTREE:
		init_appl_ll();   // Creating a completly new linked list each time ioctl call is made
	     temp = (struct atfs_u_appl *)arg;
		copy_status = copy_from_user(temp1,temp,sizeof(struct atfs_u_appl));
		printk("copystatus = %d\n", copy_status);
		if(copy_status==0)
		{	
			while(1)
			{
				//Make an entry for the application into the linked list 
				add_appl(temp1->appl_name);
				if(copy_from_user(temp2,temp1->app_pat,sizeof(struct atfs_u_acc_pat))==0)
				{
					i = 0;
					while(1)
					{
						add_acc_pat(temp1->appl_name,i,temp2->alloc_group_num);
						if(copy_from_user(temp3,temp2->pat_comp,sizeof(struct atfs_u_acc_pat_comp))==0)
						{
							while(1)
							{
							
								temp4.type = temp3->type;
								strcpy(temp4.path,temp3->path);
								temp4.estd_size = temp3->estd_size;
								add_comp_to_acc_pat(temp1->appl_name,i,temp4,inode->i_sb);
								if(temp3->next == NULL)
									break;
								copy_status=copy_from_user(temp3,temp3->next,sizeof(struct atfs_u_acc_pat_comp));
							}
						}
						i++;
						if(temp2->next == NULL)
							break;
						copy_status=copy_from_user(temp2,temp2->next,sizeof(struct atfs_u_acc_pat));
					}
				}
				//display_appl_pats(temp1->appl_name);
				if(temp1->next == NULL)
					break;
				copy_status=copy_from_user (temp1,temp1->next,sizeof(struct atfs_u_appl));
			}
		}
		return 0;
	}
	default:{
			printk("<1>no corresponding call");
			return -ENOTTY;
		}
	}
}

void display_all_apps()
{
	struct atfs_acc_pat *appl_pat_temp;
	struct atfs_appl *appl_temp;
	struct atfs_acc_pat_comp *comp_temp;
	struct list_head *appl_pat_pos,*appl_pos,*appl_pat_comp_pos;
	printk(KERN_ALERT "Inside display_all_apps \n");
	if(list_empty(&atfs_appl_ll.appl_list))
	{
		printk(KERN_ALERT "No application info present\n");
	}
	list_for_each(appl_pos,&atfs_appl_ll.appl_list)
	{
		appl_temp = list_entry(appl_pos, struct atfs_appl, appl_list);
		printk(KERN_ALERT "Application - %s\n", appl_temp->appl_name);
			list_for_each(appl_pat_pos,&(appl_temp->acc_pat1.acc_pat_list))
			{
				appl_pat_temp = list_entry(appl_pat_pos,struct atfs_acc_pat,acc_pat_list);
				printk("\tPattern no %d\n",appl_pat_temp->pat_no);
				list_for_each(appl_pat_comp_pos,&(appl_pat_temp->comp1.acc_pat_comp_list))
				{
					comp_temp = list_entry(appl_pat_comp_pos,struct atfs_acc_pat_comp,acc_pat_comp_list);
					printk("\t\t%s\n",comp_temp->path);
				}
			}
	}
}
