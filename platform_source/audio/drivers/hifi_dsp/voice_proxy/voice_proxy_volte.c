/*
 * voice_proxy_volte.c - HW voice proxy in kernel, it is used for pass through
 * voice data between AP and hifi.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330,  MA 02111-1307, USA.
 *
 */

#include "voice_proxy.h"
#include <linux/miscdevice.h>

/* lint -e528 -e753 */
#define LOG_TAG "voice_proxy_volte"
#define VOICE_PROXY_VOLTE_DATA_COUNT_MAX 100000000
#define DTS_COMP_VOICE_PROXY_VOLTE_NAME "hisilicon,voice_proxy_volte"
#define VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX 5000 /* ensure negotiation succuss during long interval */

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(x) (void)(x)
#endif

enum {
	SEC_KEY_NEGO_ENABLE,
	SEC_KEY_NEGO_SUCC_DISABLE,
	SEC_KEY_NEGO_FAIL_DISABLE,
};

/*
 * this queue is used for saving the struct of voice_proxy_lte_rx_notify when
 * the voice data is sent to tfagent. when the tfagent return the decrypted
 * voice data, proxy will get the struct of voice_proxy_lte_rx_notify and push
 * the decrypted voice data to it, finally send the this struct to hifi.
 */
LIST_HEAD(decrypting_queue);
LIST_HEAD(decrypted_queue);

/*
 * this queue is used for saving the struct of voice_proxy_lte_tx_notify when the
 * voice data is sent to tfagent. when the tfagent return the encrypted voice data,
 * proxy will get the struct of voice_proxy_lte_tx_notify and push the encrypted
 * voice data to it, finally send the this struct to hifi.
 */
LIST_HEAD(encrypting_queue);
LIST_HEAD(encrypted_queue);

/* this queue is used for saving the data which is sent to tfagent */
LIST_HEAD(pull_queue);

struct volte_priv {
	/* it is used for handling the queue of decrypting_queue/encrypting_queue */
	spinlock_t push_lock;

	/* they are used for sending data to tfagent */
	spinlock_t pull_lock;
	wait_queue_head_t pull_waitq;
	int32_t pull_wait_flag;

	/* this is used for counting the size of decrypting_queue */
	int32_t decrypting_cnt;

	/* this is used for counting the size of decrypted_queue */
	int32_t decrypted_cnt;

	/* this is used for counting the size of encrypting_queue */
	int32_t encrypting_cnt;

	/* this is used for counting the size of encrypted_queue */
	int32_t encrypted_cnt;

	/* decrypted voice data confirm */
	bool plaintext_cnf;

	/* first decrypted voice data from tfagent */
	bool first_plaintext;

	/* decrypted voice data time stamp */
	int64_t plaintext_stamp;

	/* encrypted voice data confirm */
	bool ciphertext_cnf;

	/* first encrypted voice data from tfagent */
	bool first_ciphertext;

	/* encrypted voice data time stamp */
	int64_t ciphertext_stamp;

	uint32_t rx_cnt;
	uint32_t tx_cnt;
};

static struct volte_priv priv;
static const short amrnb_frame_length[] = {
	12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0
};

static void volte_sign_init(void)
{
	priv.plaintext_cnf = false;
	priv.first_plaintext = true;
	priv.ciphertext_cnf = false;
	priv.first_ciphertext = true;
	priv.rx_cnt = 0;
	priv.tx_cnt = 0;
}

static int32_t volte_add_decrypted_data(int8_t *data, uint32_t size)
{
	struct voice_proxy_data_node *node = NULL;

	UNUSED_PARAMETER(size);

	if (priv.decrypted_cnt > VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX) {
		AUDIO_LOGE("out of queue, decrypted_cnt(%d) > QUEUE_SIZE_MAX(%d)", priv.decrypted_cnt, VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX);
		return -ENOMEM;
	}

	node = (struct voice_proxy_data_node *)data; /* lint !e826 */

	list_add_tail(&node->list_node, &decrypted_queue);
	priv.decrypted_cnt++;

	return 0;
}

static int32_t volte_add_encrypted_data(int8_t *data, uint32_t size)
{
	struct voice_proxy_data_node *node = NULL;

	UNUSED_PARAMETER(size);

	if (priv.encrypted_cnt > VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX) {
		AUDIO_LOGE("out of queue, encrypted_cnt(%d) > QUEUE_SIZE_MAX(%d)", priv.encrypted_cnt, VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX);
		return -ENOMEM;
	}

	node = (struct voice_proxy_data_node *)data; /* lint !e826 */

	list_add_tail(&node->list_node, &encrypted_queue);
	priv.encrypted_cnt++;

	return 0;
}

static int32_t volte_send_data(struct voice_proxy_data_node *node, struct send_tfagent_data *buf)
{
	int32_t ret = 0;
	struct voice_proxy_lte_rx_notify *rx = NULL;
	struct voice_proxy_lte_tx_notify *tx = NULL;

	/* fill handled voice buf to the packege of mailbox */
	if (buf->data_type == TF_TO_PROXY_DECRYPTED_DATA) { /* lint !e826 */
		rx = (struct voice_proxy_lte_rx_notify *)node->list_data.data; /* lint !e826 */
		memset(rx->data, 0, sizeof(rx->data)); /* unsafe_function_ignore: memset */
		memcpy((int8_t *)rx->data, buf->data, (size_t)PROXY_VOICE_CODEC_MAX_DATA_LEN); /* unsafe_function_ignore: memcpy */
		rx->msg_id = ID_PROXY_VOICE_LTE_RX_NTF;
		node->list_data.size = sizeof(*rx);

		ret = voice_proxy_add_data(volte_add_decrypted_data, (int8_t *)node,
			(unsigned int)sizeof(*node), ID_PROXY_VOICE_LTE_RX_NTF);
	} else {
		tx = (struct voice_proxy_lte_tx_notify *)node->list_data.data; /* lint !e826 */
		memset(tx->data, 0, sizeof(tx->data)); /* unsafe_function_ignore: memset */
		memcpy((int8_t *)tx->data, buf->data, (size_t)PROXY_VOICE_CODEC_MAX_DATA_LEN); /* unsafe_function_ignore: memcpy */
		tx->msg_id = ID_PROXY_VOICE_LTE_TX_NTF;
		node->list_data.size = sizeof(*tx);

		ret = voice_proxy_add_data(volte_add_encrypted_data, (int8_t *)node,
			(unsigned int)sizeof(*node), ID_PROXY_VOICE_LTE_TX_NTF);
	}

	if (ret) {
		AUDIO_LOGE("send node fail, ret:%d", ret);
		kfree(node);
	}

	return ret; /* lint !e438 */
}

/*
 * this function is called by tfagent kernel to return the voice data which
 * have encrypted/decrypted.
 */
int32_t proxy_push_data(void *data)
{
	struct send_tfagent_data *buf = NULL;
	struct voice_proxy_data_node *node = NULL;

	if (!data) {
		AUDIO_LOGE("param data is NULL");
		return -EINVAL;
	}

	buf = (struct send_tfagent_data *)data;
	while (1) {
		spin_lock_bh(&priv.push_lock);
		node = NULL;
		if (buf->data_type == TF_TO_PROXY_DECRYPTED_DATA) {
			if (!list_empty_careful(&decrypting_queue)) {
				node = list_first_entry(&decrypting_queue, struct voice_proxy_data_node, list_node); /* lint !e826 */

				list_del_init(&node->list_node);
				priv.decrypting_cnt--;
			}
		} else if (buf->data_type == TF_TO_PROXY_ENCRYPTED_DATA) {
			if (!list_empty_careful(&encrypting_queue)) {
				node = list_first_entry(&encrypting_queue, struct voice_proxy_data_node, list_node); /* lint !e826 */

				list_del_init(&node->list_node);
				priv.encrypting_cnt--;
			}
		} else {
			AUDIO_LOGE("data type is error, data_type:%d", buf->data_type);
			spin_unlock_bh(&priv.push_lock);
			return -EINVAL;
		}
		spin_unlock_bh(&priv.push_lock);

		if (node) {
			if (node->list_data.id < buf->id) {
				AUDIO_LOGE("ignore this node");
				kfree(node);
				node = NULL;
			} else if (node->list_data.id == buf->id) {
				volte_send_data(node, buf);
				break;
			} else {
				AUDIO_LOGE("no avail node");
				kfree(node);
				return -EINVAL;
			}
		}  else {
			AUDIO_LOGE("node is null");
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(proxy_push_data);

/*
 * this function is called by tfagent kernel to get voice data for
 * encrypt/decrypt
 */
int32_t proxy_pull_data(int8_t *data, int32_t size)
{
	int32_t ret = 0;
	struct send_tfagent_data buf;
	struct voice_proxy_data_node *node = NULL;

	if (data == NULL) {
		AUDIO_LOGE("data is null");
		return -EINVAL;
	}

	if (size < sizeof(buf)) { /*lint !e574 !e737*/
		AUDIO_LOGE("param error,size(%d)<buf size(%ld)", size, sizeof(buf));
		return -EINVAL;
	}

	memset(&buf, 0, sizeof(buf)); /* unsafe_function_ignore: memset */

	spin_lock_bh(&priv.pull_lock);
	if (list_empty_careful(&pull_queue)) {
		spin_unlock_bh(&priv.pull_lock);
		ret = wait_event_interruptible(priv.pull_waitq,
			priv.pull_wait_flag > 0); /* lint !e40 !e578 !e774 !e845 !e712 */
		if (ret) {
			if (ret != -ERESTARTSYS)
				AUDIO_LOGE("wait event interruptible fail, 0x%x", ret);

			return -EBUSY;
		}
		spin_lock_bh(&priv.pull_lock);
	}

	priv.pull_wait_flag = 0;

	if (!list_empty_careful(&pull_queue)) {
		node = list_first_entry(&pull_queue, struct voice_proxy_data_node, list_node); /* lint !e826 */

		list_del_init(&node->list_node);

		spin_unlock_bh(&priv.pull_lock);

		if (node->list_data.size <= size) {
			memcpy(data, node->list_data.data, node->list_data.size); /* lint !e732 !e747 */ /* unsafe_function_ignore: memcpy */
			ret = node->list_data.size;
		} else {
			AUDIO_LOGE("data size err, data size(%d)>size(%d)", node->list_data.size, size);
			ret = -EFAULT;
		}

		kfree(node);
	} else {
		spin_unlock_bh(&priv.pull_lock);
		ret = -EAGAIN;
	}

	return ret; /* lint !e438 */
}
EXPORT_SYMBOL(proxy_pull_data);

void proxy_wake_up_pull(void)
{
	spin_lock_bh(&priv.pull_lock);
	priv.pull_wait_flag++;
	spin_unlock_bh(&priv.pull_lock);
	wake_up(&priv.pull_waitq);
}
EXPORT_SYMBOL(proxy_wake_up_pull);

int32_t proxy_enable_sec_key_negotiation(int32_t enable)
{
	int32_t ret;
	struct voice_proxy_voice_encrypt_key_end msg;
	struct voice_proxy_data_node *node = NULL;

	if (enable != SEC_KEY_NEGO_ENABLE &&
		enable != SEC_KEY_NEGO_SUCC_DISABLE &&
		enable != SEC_KEY_NEGO_FAIL_DISABLE) {
		AUDIO_LOGE("param error, enable:%d", enable);
		return -EINVAL;
	}

	if (enable == SEC_KEY_NEGO_ENABLE) {
		msg.msg_id = ID_PROXY_VOICE_ENCRYPT_KEY_BEGIN;
	} else if (enable == SEC_KEY_NEGO_SUCC_DISABLE) {
		msg.msg_id = ID_PROXY_VOICE_ENCRYPT_KEY_END;
		msg.encrypt_negotiation_result = true;
	} else {
		msg.msg_id = ID_PROXY_VOICE_ENCRYPT_KEY_END;
		msg.encrypt_negotiation_result = false;
		AUDIO_LOGE("negotiation fail, free node");

		spin_lock_bh(&priv.push_lock);
		while (!list_empty_careful(&decrypting_queue)) {
			node = list_first_entry(&decrypting_queue,
				struct voice_proxy_data_node, list_node);
			list_del_init(&node->list_node);
			priv.decrypting_cnt--;
			kfree(node);
			node = NULL;
		}

		while (!list_empty_careful(&encrypting_queue)) {
			node = list_first_entry(&encrypting_queue,
				struct voice_proxy_data_node, list_node);
			list_del_init(&node->list_node);
			priv.encrypting_cnt--;
			kfree(node);
			node = NULL;
		}
		spin_unlock_bh(&priv.push_lock);
	}

	ret = voice_proxy_mailbox_send_msg_cb(MAILBOX_MAILCODE_ACPU_TO_HIFI_VOICE,
		msg.msg_id, &msg, (unsigned int)sizeof(msg));
	if (ret)
		AUDIO_LOGE("mailbox_send_msg fail:%d", ret);

	return ret;
}
EXPORT_SYMBOL(proxy_enable_sec_key_negotiation);

static bool is_queue_range_valid(int32_t type)
{
	if (type == PROXY_TO_TF_UNDECRYPT_DATA) {
		if (priv.decrypting_cnt > VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX) {
			AUDIO_LOGE("out of queue, decrypting_cnt(%d)>(%d)",
				priv.decrypting_cnt, VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX);
			return false;
		}
	} else {
		if (priv.encrypting_cnt > VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX) {
			AUDIO_LOGE("out of queue, encrypting_cnt(%d)>(%d)",
				priv.encrypting_cnt, VOICE_PROXY_VOLTE_QUEUE_SIZE_MAX);
			return false;
		}
	}

	return true;
}

static void add_data_to_queue(struct voice_proxy_data_node *node, int32_t type)
{
	spin_lock_bh(&priv.push_lock);
	if (type == PROXY_TO_TF_UNDECRYPT_DATA) {
		list_add_tail(&node->list_node, &decrypting_queue);
		priv.decrypting_cnt++;
	} else {
		list_add_tail(&node->list_node, &encrypting_queue);
		priv.encrypting_cnt++;
	}
	spin_unlock_bh(&priv.push_lock);
}

static void send_data_to_tfagent(struct voice_proxy_data_node *tf_node)
{
	spin_lock_bh(&priv.pull_lock);
	list_add_tail(&tf_node->list_node, &pull_queue);
	priv.pull_wait_flag++;
	spin_unlock_bh(&priv.pull_lock);
	wake_up(&priv.pull_waitq);

	if (priv.rx_cnt >= VOICE_PROXY_VOLTE_DATA_COUNT_MAX)
		priv.rx_cnt = 0;

	if (priv.tx_cnt >= VOICE_PROXY_VOLTE_DATA_COUNT_MAX)
		priv.tx_cnt = 0;
}

static int get_voice_data(int8_t *rev_buf, struct send_tfagent_data *buf, struct voice_proxy_data_node *node)
{
	struct voice_proxy_lte_rx_notify *rx = NULL;
	struct voice_proxy_lte_tx_notify *tx = NULL;

	if (buf->data_type == PROXY_TO_TF_UNDECRYPT_DATA) {
		rx = (struct voice_proxy_lte_rx_notify *)rev_buf; /* lint !e826 */
		if ((rx->codec_type != 0) ||
			(rx->frame_type >= ARRAY_SIZE(amrnb_frame_length)) ||
			((rx->frame_type < ARRAY_SIZE(amrnb_frame_length)) &&
			(amrnb_frame_length[rx->frame_type] > PROXY_VOICE_CODEC_MAX_DATA_LEN))) {
			AUDIO_LOGE("rx frame info error, codec_type, frame type:%d, %d",
				rx->codec_type, rx->frame_type);
			kfree(node);
			return -EINVAL;
		}
		buf->buf_size = amrnb_frame_length[rx->frame_type];
		memcpy(buf->data, (int8_t *)rx->data, buf->buf_size); /* lint !e732 !e747 */ /* unsafe_function_ignore: memcpy */
		node->list_data.id = priv.rx_cnt;
		buf->id = node->list_data.id;
		priv.rx_cnt++;
	} else {
		tx = (struct voice_proxy_lte_tx_notify *)rev_buf; /* lint !e826 */
		if (tx->frame_type >= ARRAY_SIZE(amrnb_frame_length) ||
			((tx->frame_type < ARRAY_SIZE(amrnb_frame_length)) &&
			(amrnb_frame_length[tx->frame_type] > PROXY_VOICE_CODEC_MAX_DATA_LEN))) {
			AUDIO_LOGE("tx frame type error %d", tx->frame_type);
			kfree(node);
			return -EINVAL;
		}
		buf->buf_size = amrnb_frame_length[tx->frame_type];
		memcpy(buf->data, (int8_t *)tx->data, buf->buf_size); /* lint !e732 !e747 */ /* unsafe_function_ignore: memcpy */
		node->list_data.id = priv.tx_cnt;
		buf->id = node->list_data.id;
		priv.tx_cnt++;
	}

	return 0;
}

static int32_t volte_add_pull_data(int8_t *rev_buf, uint32_t buf_size, int32_t type)
{
	int32_t ret;
	struct send_tfagent_data buf;
	struct voice_proxy_data_node *node = NULL;
	struct voice_proxy_data_node *tf_node = NULL;

	if (!is_queue_range_valid(type))
		goto OUT;

	ret = voice_proxy_create_data_node(&node, rev_buf, (int)buf_size);
	if (ret) {
		AUDIO_LOGE("kzalloc node failed %d", ret);
		return ret;
	}

	buf.data_type = type;
	memset(buf.data, 0, sizeof(buf.data)); /* unsafe_function_ignore: memset */

	/*
	 * 1.get the voice data from the structure of
	 * voice_proxy_lte_rx_notify/voice_proxy_lte_tx_notify
	 */
	ret = get_voice_data(rev_buf, &buf, node);
	if (ret)
		goto OUT;

	ret = voice_proxy_create_data_node(&tf_node, (int8_t *)&buf, (int)sizeof(buf));
	if (ret) {
		AUDIO_LOGE("kzalloc push_node failed %d", ret);
		kfree(node);
		goto OUT;
	}

	/*
	 * 2.save the structure for decrypted/encrypted voice
	 * data and then send the structure to hifi
	 */
	add_data_to_queue(node, type);

	/* 3.send voice data to tfagent for decrypting/encrypting */
	send_data_to_tfagent(tf_node);

	return ret;

OUT:
	spin_lock_bh(&priv.pull_lock);
	priv.pull_wait_flag++;
	spin_unlock_bh(&priv.pull_lock);
	wake_up(&priv.pull_waitq);

	return -ENOMEM;
}

static void volte_receive_undecrypt_ntf(int8_t *rev_buf, uint32_t buf_size)
{
	int32_t ret;

	if (!rev_buf) {
		AUDIO_LOGE("receive_undecrypt_ntf fail, param rev_buf is NULL");
		return;
	}

	AUDIO_LOGE("volte_receive_undecrypt_ntf");

	ret = volte_add_pull_data(rev_buf, buf_size, PROXY_TO_TF_UNDECRYPT_DATA);
	if (ret) {
		AUDIO_LOGE("volte_add_pull_data failed");
		return;
	}

	voice_proxy_add_work_queue_cmd(ID_PROXY_VOICE_LTE_RX_CNF, VOICE_MC_MODEM0, 0);
}

static void volte_receive_unencrypt_ntf(int8_t *rev_buf, uint32_t buf_size)
{
	int32_t ret;

	if (!rev_buf) {
		AUDIO_LOGE("receive_unencrypt_ntf fail, param rev_buf is NULL");
		return;
	}

	ret = volte_add_pull_data(rev_buf, buf_size, PROXY_TO_TF_UNENCRYPT_DATA);
	if (ret) {
		AUDIO_LOGE("volte_add_pull_data failed");
		return;
	}

	voice_proxy_add_work_queue_cmd(ID_PROXY_VOICE_LTE_TX_CNF, VOICE_MC_MODEM0, 0);
}

static void volte_receive_decrypted_cnf(int8_t *rev_buf, uint32_t buf_size)
{
	int32_t ret;

	AUDIO_LOGE("volte_receive_decrypted_cnf");

	ret = voice_proxy_add_cmd(ID_VOICE_PROXY_LTE_RX_CNF);
	if (ret)
		AUDIO_LOGE("send plaintext data cnf failed");
}

static void volte_receive_encrypted_cnf(int8_t *rev_buf, uint32_t buf_size)
{
	int32_t ret;

	AUDIO_LOGE("volte_receive_encrypted_cnf");

	ret = voice_proxy_add_cmd(ID_VOICE_PROXY_LTE_TX_CNF);
	if (ret)
		AUDIO_LOGE("send ciphertext data cnf failed");
}

static void volte_get_decryped_data(int8_t *data, uint32_t *size)
{
	struct voice_proxy_data_node *node = NULL;

	if (!list_empty_careful(&decrypted_queue)) {
		node = list_first_entry(&decrypted_queue, struct voice_proxy_data_node, list_node); /* lint !e826 */

		if (*size < (uint32_t)node->list_data.size) {
			AUDIO_LOGE("Size is invalid, size = %d, list_data.size = %d", *size, node->list_data.size);
			return;
		}

		*size = (uint32_t)node->list_data.size;
		memcpy(data, node->list_data.data, (size_t)*size); /* unsafe_function_ignore: memcpy */

		list_del_init(&node->list_node);
		kfree(node);

		priv.decrypted_cnt--;
		priv.first_plaintext = false;
		priv.plaintext_cnf = false;
	} else {
		*size = 0;
	}
} /* lint !e438 */

static void volte_get_encrypted_data(int8_t *data, uint32_t *size)
{
	struct voice_proxy_data_node *node = NULL;

	if (!list_empty_careful(&encrypted_queue)) {
		node = list_first_entry(&encrypted_queue, struct voice_proxy_data_node, list_node); /* lint !e826 */

		if (*size < (uint32_t)node->list_data.size) {
			AUDIO_LOGE("Size is invalid, size = %d, list_data.size = %d", *size, node->list_data.size);
			return;
		}

		*size = (uint32_t)node->list_data.size;
		memcpy(data, node->list_data.data, (size_t)*size); /* unsafe_function_ignore: memcpy */

		list_del_init(&node->list_node);
		kfree(node);

		priv.encrypted_cnt--;
		priv.first_ciphertext = false;
		priv.ciphertext_cnf = false;
	} else {
		*size = 0;
	}
} /* lint !e438 */


static void volte_handle_decrypted_ntf(int8_t *data, uint32_t *size, uint16_t *msg_id)
{
	if (!data || !size || !msg_id) {
		AUDIO_LOGE("handle_decrypted_ntf fail, param is NULL");
		return;
	}

	voice_proxy_set_send_sign(priv.first_plaintext, &priv.plaintext_cnf, &priv.plaintext_stamp);

	if (priv.first_plaintext || priv.plaintext_cnf)
		volte_get_decryped_data(data, size);
	else
		*size = 0;

	*msg_id = ID_PROXY_VOICE_LTE_RX_NTF;
}

static void volte_handle_decrypted_cnf(int8_t *data, uint32_t *size, uint16_t *msg_id)
{
	if (!data || !size || !msg_id) {
		AUDIO_LOGE("handle_decrypted_cnf fail, param is NULL");
		return;
	}

	priv.plaintext_cnf = true;
	priv.plaintext_stamp = voice_proxy_get_time_ms();

	volte_get_decryped_data(data, size);
	*msg_id = ID_PROXY_VOICE_LTE_RX_NTF;
}

static void volte_handle_encrypted_ntf(int8_t *data, uint32_t *size, uint16_t *msg_id)
{
	if (!data || !size || !msg_id) {
		AUDIO_LOGE("handle_encrypted_ntf fail, param is NULL");
		return;
	}

	voice_proxy_set_send_sign(priv.first_ciphertext, &priv.ciphertext_cnf, &priv.ciphertext_stamp);

	if (priv.first_ciphertext || priv.ciphertext_cnf)
		volte_get_encrypted_data(data, size);
	else
		*size = 0;

	*msg_id = ID_PROXY_VOICE_LTE_TX_NTF;
}

static void volte_handle_encrypted_cnf(int8_t *data, uint32_t *size, uint16_t *msg_id)
{
	if (!data || !size || !msg_id) {
		AUDIO_LOGE("handle_encrypted_cnf fail, param is NULL");
		return;
	}

	priv.ciphertext_cnf = true;
	priv.ciphertext_stamp = voice_proxy_get_time_ms();

	volte_get_encrypted_data(data, size);
	*msg_id = ID_PROXY_VOICE_LTE_TX_NTF;
}

static int volte_probe(struct platform_device *pdev)
{
	int32_t ret = 0;

	memset(&priv, 0, sizeof(priv)); /* unsafe_function_ignore: memset */

	AUDIO_LOGI("voice proxy volte prob,pdev name[%s]", pdev->name);

	priv.pull_wait_flag = 0;

	spin_lock_init(&priv.push_lock);
	spin_lock_init(&priv.pull_lock);
	init_waitqueue_head(&priv.pull_waitq);

	volte_sign_init();

	voice_proxy_register_msg_callback(ID_VOICE_PROXY_LTE_RX_NTF, volte_receive_undecrypt_ntf);
	voice_proxy_register_msg_callback(ID_VOICE_PROXY_LTE_TX_NTF, volte_receive_unencrypt_ntf);
	voice_proxy_register_msg_callback(ID_VOICE_PROXY_LTE_RX_CNF, volte_receive_decrypted_cnf);
	voice_proxy_register_msg_callback(ID_VOICE_PROXY_LTE_TX_CNF, volte_receive_encrypted_cnf);
	voice_proxy_register_cmd_callback(ID_PROXY_VOICE_LTE_RX_NTF, volte_handle_decrypted_ntf);
	voice_proxy_register_cmd_callback(ID_PROXY_VOICE_LTE_TX_NTF, volte_handle_encrypted_ntf);
	voice_proxy_register_cmd_callback(ID_VOICE_PROXY_LTE_RX_CNF, volte_handle_decrypted_cnf);
	voice_proxy_register_cmd_callback(ID_VOICE_PROXY_LTE_TX_CNF, volte_handle_encrypted_cnf);
	voice_proxy_register_sign_init_callback(volte_sign_init);

	return ret;
}

static int volte_remove(struct platform_device *pdev)
{

	UNUSED_PARAMETER(pdev);

	voice_proxy_deregister_msg_callback(ID_VOICE_PROXY_LTE_RX_NTF);
	voice_proxy_deregister_msg_callback(ID_VOICE_PROXY_LTE_TX_NTF);
	voice_proxy_deregister_msg_callback(ID_VOICE_PROXY_LTE_RX_CNF);
	voice_proxy_deregister_msg_callback(ID_VOICE_PROXY_LTE_TX_CNF);

	voice_proxy_deregister_cmd_callback(ID_PROXY_VOICE_LTE_RX_NTF);
	voice_proxy_deregister_cmd_callback(ID_PROXY_VOICE_LTE_TX_NTF);
	voice_proxy_deregister_cmd_callback(ID_VOICE_PROXY_LTE_RX_CNF);
	voice_proxy_deregister_cmd_callback(ID_VOICE_PROXY_LTE_TX_CNF);

	voice_proxy_deregister_sign_init_callback(volte_sign_init);

	return 0;
}

static const struct of_device_id volte_match_table[] = {
	{
		.compatible = DTS_COMP_VOICE_PROXY_VOLTE_NAME,
		.data = NULL,
	},
	{
	} /* lint !e785 */
};

static struct platform_driver volte_driver = {
	.driver = {
		.name  = "voice proxy volte",
		.owner = THIS_MODULE, /* lint !e64 */
		.of_match_table = of_match_ptr(volte_match_table),
	}, /* lint !e785 */
	.probe = volte_probe,
	.remove = volte_remove,
}; /* lint !e785 */

static int __init volte_init(void)
{
	int32_t ret;

	AUDIO_LOGI("Audio:voice proxy volte init");

	ret = platform_driver_register(&volte_driver); /* lint !e64 */
	if (ret)
		AUDIO_LOGE("voice proxy volte driver register fail,ERROR is %d", ret);

	return ret;
}

static void __exit volte_exit(void)
{
	platform_driver_unregister(&volte_driver);
}

module_init(volte_init);
module_exit(volte_exit);

MODULE_DESCRIPTION("voice proxy volte driver");
MODULE_LICENSE("GPL");
