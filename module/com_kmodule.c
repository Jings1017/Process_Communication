#include "com_kmodule.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/net.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#define NETLINK_USER 31

struct sock *nl_sk = NULL;
struct mailbox mail[1001];
struct nlmsghdr *nlh;
struct sk_buff *skb_out;
struct msg_data *msg_data,*temp;

int res,i,pid;
int user_pid, target_id;
int same_account=0, account_num=0;
int account[1000];
int operation[1000];

char app_msg[1000],str[500];
char statement[13],id_text[6];
char *id_end;


static void hello_nl_recv_msg(struct sk_buff *skb)
{
    // get message from app
    nlh=(struct nlmsghdr*)skb->data;
    pid = nlh->nlmsg_pid;
    strcpy(app_msg, (char *)nlmsg_data(nlh));

    strncpy(statement,app_msg,12);
    // registration
    if(strcmp(statement,"Registration")==0)
    {
        strncpy(id_text,&app_msg[16],4);
        sscanf(id_text,"%d",&user_pid);
        operation[pid]=user_pid;

        for(i=0; i<account_num; i++)
        {
            if(account[i]==user_pid)
            {
                // return message to app
                memset(app_msg,0,1000);
                strcpy(app_msg,"Fail");
                skb_out = nlmsg_new(strlen(app_msg),0);
                if(!skb_out)
                {
                    printk(KERN_ERR "allocate netlink message failed\n");
                    return;
                }
                nlh=nlmsg_put(skb_out,0,1,NLMSG_DONE,strlen(app_msg),0);

                NETLINK_CB(skb_out).dst_group = 0;
                strcpy(nlmsg_data(nlh),app_msg);
                res=nlmsg_unicast(nl_sk,skb_out,pid);
                printk(KERN_INFO "%s\n",(char*)NLMSG_DATA((struct nlmsghdr*)skb_out->data));
                if(res<0)
                {
                    printk(KERN_INFO "netlink message unicast failed\n");
                }
                return;
            }
        }

        account[account_num++]=user_pid;

        strcpy(str,&app_msg[26]);
        // queueud
        if(strcmp(str,"queued")==0)
        {
            mail[user_pid].type = 1;
            mail[user_pid].msg_data_count = 0;
            mail[user_pid].msg_data_tail=(struct msg_data*)kmalloc(sizeof(struct msg_data),GFP_KERNEL);
            mail[user_pid].msg_data_head=mail[user_pid].msg_data_tail;
            mail[user_pid].msg_data_head->next = (struct msg_data*)kmalloc(sizeof(struct msg_data),GFP_KERNEL);
            mail[user_pid].msg_data_head->next->next = (struct msg_data*)kmalloc(sizeof(struct msg_data),GFP_KERNEL);
            mail[user_pid].msg_data_head->next->next->next = mail[user_pid].msg_data_head;
            mail[user_pid].msg_data_tail->next = mail[user_pid].msg_data_head->next;
        }
        // unqueued
        else if(strcmp(str,"unqueued")==0)
        {
            mail[user_pid].type = 0;
            mail[user_pid].msg_data_count = 0;
            mail[user_pid].msg_data_tail=(struct msg_data*)kmalloc(sizeof(struct msg_data),GFP_KERNEL);
            mail[user_pid].msg_data_head=mail[user_pid].msg_data_tail;
        }
        // return message to app
        memset(app_msg,0,1000);
        strcpy(app_msg,"Success");
        printk("%c%d",mail[user_pid].type,mail[user_pid].type);
    }
    // Send or Recv
    else
    {
        strncpy(id_text,&app_msg[8],4);
        sscanf(id_text,"%d",&target_id);


        memset(statement,0,12);
        strncpy(statement,app_msg,4);
        // Recv
        if(strcmp(statement,"Recv")==0)
        {
            // unqueued
            if(mail[operation[pid]].type==0)
            {
                // the box in this id has data
                if(mail[operation[pid]].msg_data_count!=0)
                {
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,mail[operation[pid]].msg_data_head->buf);
                }
                else
                {
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,"Fail");
                }
            }
            // queued
            else if(mail[operation[pid]].type ==1)
            {
                if(mail[operation[pid]].msg_data_count!=0)
                {
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,mail[operation[pid]].msg_data_head->buf);
                    strcpy(mail[operation[pid]].msg_data_head->buf,"");
                    mail[operation[pid]].msg_data_count -= 1;
                    mail[operation[pid]].msg_data_head = mail[operation[pid]].msg_data_head->next;
                }
                else
                {
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,"Fail");
                }
            }
        }
        // Send
        else if(strcmp(statement,"Send")==0)
        {
            strncpy(statement,&app_msg[8],4);
            sscanf(statement,"%d",&target_id);



            // handle exception
            for(i=0; i<account_num; i++)
            {
                if(account[i]==target_id)
                {
                    same_account++;
                }
            }
            if(same_account==0)
            {
                memset(app_msg,0,sizeof(app_msg));
                strcpy(app_msg,"Fail");
                skb_out = nlmsg_new(strlen(app_msg),0);
                if(!skb_out)
                {
                    printk(KERN_ERR "allocate netlink message failed\n");
                    return;
                }
                nlh=nlmsg_put(skb_out,0,1,NLMSG_DONE,strlen(app_msg),0);

                NETLINK_CB(skb_out).dst_group = 0;
                strcpy(nlmsg_data(nlh),app_msg);
                res=nlmsg_unicast(nl_sk,skb_out,pid);
                printk(KERN_INFO "%s\n",(char*)NLMSG_DATA((struct nlmsghdr*)skb_out->data));
                if(res<0)
                {
                    printk(KERN_INFO "netlink message unicast failed\n");
                }
                return;
            }

            memset(str,0,sizeof(str));
            strncpy(str,&app_msg[17],255);
            // string too long
            if(strlen(str)>=256)
            {
                memset(app_msg,0,sizeof(app_msg));
                strcpy(app_msg,"Fail");
                //goto ack;
                skb_out = nlmsg_new(strlen(app_msg),0);
                if(!skb_out)
                {
                    printk(KERN_ERR "allocate netlink message failed\n");
                    return;
                }
                nlh=nlmsg_put(skb_out,0,1,NLMSG_DONE,strlen(app_msg),0);

                NETLINK_CB(skb_out).dst_group = 0;
                strcpy(nlmsg_data(nlh),app_msg);
                res=nlmsg_unicast(nl_sk,skb_out,pid);
                printk(KERN_INFO "%s\n",(char*)NLMSG_DATA((struct nlmsghdr*)skb_out->data));
                if(res<0)
                {
                    printk(KERN_INFO "netlink message unicast failed\n");
                }
                return;
            }

            // uqueued
            if(mail[target_id].type==0)
            {
                strcpy(mail[target_id].msg_data_head->buf,str);
                mail[target_id].msg_data_count=1;
                //printk("%d",mail[target_id].msg_data_count);
                memset(app_msg,0,sizeof(app_msg));
                strcpy(app_msg,"Success");
            }
            // queued
            else if(mail[target_id].type==1)
            {
                // over max
                if(mail[target_id].msg_data_count>=3)
                {
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,"Fail");
                    skb_out = nlmsg_new(strlen(app_msg),0);
                    if(!skb_out)
                    {
                        printk(KERN_ERR "allocate netlink message failed\n");
                        return;
                    }
                    nlh=nlmsg_put(skb_out,0,1,NLMSG_DONE,strlen(app_msg),0);

                    NETLINK_CB(skb_out).dst_group = 0;
                    strcpy(nlmsg_data(nlh),app_msg);

                    res=nlmsg_unicast(nl_sk,skb_out,pid);
                    printk(KERN_INFO "%s\n",(char*)NLMSG_DATA((struct nlmsghdr*)skb_out->data));

                    if(res<0)
                    {
                        printk(KERN_INFO "netlink message unicast failed\n");
                    }
                    return;
                }
                else
                {
                    strcpy(mail[target_id].msg_data_tail->buf,str);
                    mail[target_id].msg_data_count++;
                    mail[target_id].msg_data_tail = mail[target_id].msg_data_tail->next;
                    memset(app_msg,0,sizeof(app_msg));
                    strcpy(app_msg,"Success");
                }
            }
        }
    }

    skb_out = nlmsg_new(strlen(app_msg),0);
    if(!skb_out)
    {
        printk(KERN_ERR "allocate netlink message failed\n");
        return;
    }
    nlh=nlmsg_put(skb_out,0,1,NLMSG_DONE,strlen(app_msg),0);

    NETLINK_CB(skb_out).dst_group = 0;
    strcpy(nlmsg_data(nlh),app_msg);

    res=nlmsg_unicast(nl_sk,skb_out,pid);
    printk(KERN_INFO "%s\n",(char*)NLMSG_DATA((struct nlmsghdr*)skb_out->data));

    if(res<0)
    {
        printk(KERN_INFO "netlink message unicast failed\n");
    }
}

static int __init com_kmodule_init(void)
{
    printk("============ hello com_kmodule ===========\n");
    struct netlink_kernel_cfg cfg =
    {
        .input = hello_nl_recv_msg,
    };
    nl_sk=netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if(!nl_sk)
    {
        printk(KERN_ALERT "socket create error! .\n");
        return -10;
    }

    return 0;
}

static void __exit com_kmodule_exit(void)
{
    printk(KERN_INFO "=========== Exit com_kmodule. Bye~ ===========\n");
    netlink_kernel_release(nl_sk);
}

module_init(com_kmodule_init);
module_exit(com_kmodule_exit);
