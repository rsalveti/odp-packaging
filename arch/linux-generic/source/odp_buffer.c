/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of Linaro Limited nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIALDAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <odp_buffer.h>
#include <odp_buffer_internal.h>

#include <string.h>
#include <stdio.h>






void *odp_buffer_get_addr(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = odp_buf_to_hdr(buf);

	return hdr->addr;
}


size_t odp_buffer_get_size(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = odp_buf_to_hdr(buf);

	return hdr->size;
}


int odp_buffer_is_scatter(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = odp_buf_to_hdr(buf);

	if (hdr->scatter.num_bufs == 0)
		return 0;
	else
		return 1;
}


int odp_buffer_is_valid(odp_buffer_t buf)
{
	odp_buffer_bits_t handle;

	handle.u32 = buf;

	return (handle.index != ODP_BUFFER_INVALID_INDEX);
}


int odp_buffer_snprint(char *str, size_t n, odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr;
	int len = 0;

	if (!odp_buffer_is_valid(buf)) {
		printf("Buffer is not valid.\n");
		return len;
	}

	hdr = odp_buf_to_hdr(buf);

	len += snprintf(&str[len], n-len,
			"Buffer\n");
	len += snprintf(&str[len], n-len,
			"  pool         %i\n",        hdr->pool);
	len += snprintf(&str[len], n-len,
			"  index        %"PRIu32"\n", hdr->index);
	len += snprintf(&str[len], n-len,
			"  phy_addr     %"PRIu64"\n", hdr->phy_addr);
	len += snprintf(&str[len], n-len,
			"  addr         %p\n",        hdr->addr);
	len += snprintf(&str[len], n-len,
			"  size         %zu\n",       hdr->size);
	len += snprintf(&str[len], n-len,
			"  cur_offset   %zu\n",       hdr->cur_offset);
	len += snprintf(&str[len], n-len,
			"  ref_count    %i\n",        hdr->ref_count);
	len += snprintf(&str[len], n-len,
			"  type         %i\n",        hdr->type);
	len += snprintf(&str[len], n-len,
			"  Scatter list\n");
	len += snprintf(&str[len], n-len,
			"    num_bufs   %i\n",        hdr->scatter.num_bufs);
	len += snprintf(&str[len], n-len,
			"    pos        %i\n",        hdr->scatter.pos);
	len += snprintf(&str[len], n-len,
			"    total_len  %zu\n",       hdr->scatter.total_len);

	return len;
}



void odp_buffer_print(odp_buffer_t buf)
{
	#define STR_LEN 512
	char str[STR_LEN];
	int len;

	len = odp_buffer_snprint(str, STR_LEN-1, buf);
	str[len] = 0;

	printf("\n%s\n", str);
}





