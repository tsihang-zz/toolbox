#include "oryx.h"
#include "rfc.h"
#if 0
static list_eq_t* pnode_eqs = NULL;
static list_eq_t* pnode_map_eqs = NULL;
static list_eq_t* pnoder_eqs = NULL;

filtset_t ipv4_filtset;

vlib_rfc_main_t vlib_rfc_main;

static __oryx_always_inline__
void bmpset(uint32_t *tbmp, uint32_t i, bool value)
{
    uint32_t k, pos;
    uint32_t temp = 1;
    
    k = SIZE-1 - (i/LENGTH);
    pos = i % LENGTH;
    temp <<= pos;	

    if (value == TRUE)
    {
        tbmp[k] |= temp;
    }
    else
    {
        temp = ~temp;
        tbmp[k] &= temp;
    }

    return ;
}

static __oryx_always_inline__
bool bmpcmp(uint32_t *abmp, uint32_t *bbmp)
{
    int32_t i;
	
    for (i = SIZE-1; i >= 0; i--)
    {
        if ((*(abmp+i)) != (*(bbmp+i)))
	{
	    return FALSE;
	}
    }

    return TRUE;
}
/*******************************************************************************
   @Function Name:   bmpquery
   @Description:     search bmp in eqs, called by rfc_set_phase0_dim_cell
   @return:          if tbmp not exist in eqs, return -1
		     else return eqid of ces whose cbm match tbmp
********************************************************************************/
static __oryx_always_inline__
int32_t  bmpquery(list_eq_t *eqs, uint32_t *tbmp)
{
    int32_t i;
	
    for (i = 0; i < eqs->n_ces; i++)
    {
        if (bmpcmp(eqs->ces[i].cbm, tbmp))
        {
            return i;
        }
    }

    return -1;
}

/*******************************************************************************
   @Function Name:   add_list_eqs_ces
   @Description:     add new ces to eqs, called by rfc_set_phase0_dim_cell
   @return:          the eqid of the new ces
********************************************************************************/
static __oryx_always_inline__
uint32_t add_list_eqs_ces(list_eq_t *eqs, uint32_t *tbmp)
{
    uint32_t i;
	
    eqs->ces[eqs->n_ces].eqid = eqs->n_ces;
    
    for (i = 0; i < SIZE; i++)
    {
        eqs->ces[eqs->n_ces].cbm[i] = tbmp[i];
    }

    eqs->n_ces++;
	
    return  eqs->n_ces -1;
}

static __oryx_always_inline__
void read_ip_range_by_mask(rfc_ip_addr_t ip, uint32_t mask, uint32_t* high_range, uint32_t* low_range)
{
	/* assumes IPv4 prefixes */
	uint32_t trange[4]; 
	uint32_t ptrange[4];
	uint32_t i;
	uint32_t masklit1;
	uint32_t masklit2, masklit3;

	trange[0] = ip.addr_v4>>24;
	trange[1] = ip.addr_v4<<8>>24;
	trange[2] = ip.addr_v4<<16>>24;
	trange[3] = ip.addr_v4<<24>>24;

	if (mask == 0)	//match all IP
	{
		/*start IP is 0.0.0.0*/ 
		high_range[0] = 0;
		low_range[0]  = 0;

		/*end IP is 255.255.255.255*/
		high_range[1] = 0xffff;
		low_range[1]  = 0xffff;

		return;
	}
	
	mask = 32 - mask;
	masklit1 = mask / 8;
	masklit2 = mask % 8;
	
	for (i = 0; i < 4; i++)
		ptrange[i] = trange[i];

	/* count the start IP */
	for (i = 3; i > 3-masklit1; i--) {
			ptrange[i] = 0;
	}

	if (masklit2 != 0) {
		masklit3 = 1;
		masklit3 <<= masklit2;
		masklit3 -= 1;
		masklit3 = ~masklit3;
		ptrange[3-masklit1] &= masklit3;
	}
	
	/* store start IP */
	high_range[0] = ptrange[0];
	high_range[0] <<= 8;
	high_range[0] += ptrange[1];
	low_range[0] = ptrange[2];
	low_range[0] <<= 8;
	low_range[0] += ptrange[3];
	
	/* count the end IP */
	for (i = 3; i > 3-masklit1; i--) {
		ptrange[i] = BYTE_MAX_VALUE;
	}

	if (masklit2 != 0) {
		masklit3 = 1;
		masklit3 <<= masklit2;
		masklit3 -= 1;
		ptrange[3-masklit1] |= masklit3;
	}
	
	/* store end IP */
	high_range[1] = ptrange[0];
	high_range[1] <<= 8;
	high_range[1] += ptrange[1];
	low_range[1] = ptrange[2];
	low_range[1] <<= 8;
	low_range[1] += ptrange[3];
}

static __oryx_always_inline__
void read_ip_range_by_start_end_point(rfc_ip_addr_t start_ip, rfc_ip_addr_t end_ip, uint32_t* high_range, uint32_t* low_range)
{
    /*start ip*/
    high_range[0] = start_ip.addr_v4 >> 16;
    low_range[0] = start_ip.addr_v4 << 16 >> 16;

    /*end ip*/
    high_range[1] = end_ip.addr_v4 >> 16;
    low_range[1] = end_ip.addr_v4 << 16 >> 16;

    return;
}
/*******************************************************************************
  @Function Name:   get_high_pri_rule_indx
  @Description:     Get rule id with highest priority, 
		    called by rfc_set_phase2_cell
  @return:          ruld id with highest priority
********************************************************************************/
static __oryx_always_inline__
uint32_t get_high_pri_rule_indx(uint32_t *tbmp)
{
    int32_t  k;
    uint32_t temp_value;
    uint32_t base;
	
    for (k = SIZE-1; k >= 0; k--)
    {
        temp_value = tbmp[k];
        base = LENGTH*(SIZE-1-k);

	if (temp_value == 0)
        {
            continue;
        }

        if (temp_value & 0xFFFF) //bit0-15
        {
	    if (temp_value & 0xFF)//bit0-7
	    {
	        if (temp_value & 0xF)//bit0-3
		{
		    if (temp_value &0x3)//bit0-1
		    {
			if (temp_value & 0x1)//bit 0
                        {
                            return base + 1;
                        }
		        else//bit1
                        {
                            return base + 2;
                        }
		    }
		    else //bit2-3
		    {
		        if (temp_value & 0x4)//bit2
                        {
                            return base + 3;
                        }
			else//bit3
                        {
                            return base + 4;
                        }
		    }
		}
		else//bit4-7 
		{
		    if (temp_value & 0x30)//bit4-5
		    {
		        if (temp_value & 0x10)//bit4
                        {
                            return base + 5;
                        }
		        else//bit5
                        {
                            return base + 6;
                        }
		    }
		    else//bit 6-7
		    {
		        if (temp_value & 0x40)//bit6
                        {
                            return base + 7;
                        }
		        else//bit7
                        {
                            return base + 8;	
                        }
		    }
		}
	    }
	    else //bit8-15
	    {
	        if (temp_value & 0xf00)//bit8-11
	        {
		    if (temp_value & 0x300)//bit8-9
		    {
		        if (temp_value & 0x100)//bit8
                        {
                            return base + 9;
                        }
			else //bit9
                        {
                            return base + 10;
                        }
		    }
		    else//bit10-11
		    {
		        if (temp_value & 0x400)//bit10
                        {
                            return base + 11;
                        }
		        else//bit11
                        {
                            return base + 12;
                        }
		    }
		}
		else//bit 12-15
		{
		    if (temp_value & 0x3000)//bit 12-13
		    {
		        if (temp_value & 0x1000)
                        {
                            return base + 13;
                        }
			else
                        {
                            return base + 14;
                        }
		    }
		    else//bit14-15
		    {
		        if (temp_value & 0x4000)//bit14
                        {
                            return base + 15;
                        }
			else//bit 16
                        {
                            return base + 16;
                        }
		    }
		}
	    }
	}
	else //bit 16-31
	{
	    if (temp_value & 0xFF0000)//bit16-23
	    {
	        if (temp_value & 0xF0000)//bit16-19
	        {
	            if (temp_value & 0x30000)//bit16-17
	            {
		        if (temp_value & 0x10000)//bit16
                        {
                            return base + 17;
                        }
		        else
                        {
                            return base + 18;
                        }
		    }
		    else//bit18-19
		    {
		        if (temp_value & 0x40000)
                        {
                            return base + 19;
                        }
		        else
                        {
                            return base + 20;
                        }
		    }
		}
	        else//bit20-23
	        {
		    if (temp_value & 0x300000)//bit20-21
		    {
		        if (temp_value & 0x100000)//bit20
                        {
                            return base + 21;
                        }
		        else
                        {
                            return base + 22;
                        }
		    }
		    else//bit22-23
		    {
		        if (temp_value & 0x400000)//bit22
                        {
                            return base + 23;
                        }
		        else
                        {
                            return base + 24;
                        }
		    }
		}
	    }
	    else//bit 24-31
	    {
	        if (temp_value & 0xF000000)//bit 24-27
	        {
		    if (temp_value & 0x3000000)//bit24-25
		    {
		        if (temp_value & 0x1000000)//bit24
                        {
                            return base + 25;
                        }
			else//bit25
                        {
                            return base + 26;
                        }
		    }
		    else//bit26-27
		    {
		        if (temp_value & 0x4000000)//bit26
                        {
                            return base + 27;
                        }
			else
                        {
                            return base + 28;
                        }
		    }
		}
		else//bit28-31
		{
		    if (temp_value & 0x30000000)//bit28-29
		    {
		        if (temp_value & 0x10000000)//bit28
                        {
                            return base + 29;
                        }
			else
                        {
                            return base + 30;
                        }
		    }
		    else//bit30-31
		    {
		        if (temp_value & 0x40000000)//bit30
                        {
                            return base + 31;
                        }
			else
                        {
                            return base + 32;
                        }
		    }
		}
	    }	    
	}		
    }
    
    return RFC_INVALID_RULE_ID;
}

/*******************************************************************************
  @Function Name:   rfc_format_filter
  @Description:     format user's filter rule into filtset_t used to encode rfc 
		    node
  @param: rule	    input: point to user's filter rule
  @param: rule id   input: user's filter rule id
  @param: rule id   output: use to save formated rule
********************************************************************************/
void rfc_format_filter(rfc_5tuple_t *rule, uint32_t rule_id)
{
	filtset_t *ip_filtset = &ipv4_filtset;
	
    filter_t *tempfilt, tempfilt1;

    tempfilt = &tempfilt1;

    /* sip */
    if(rule->sip_mask != RFC_INVALID_IP_MASK_LEN)
    {
        read_ip_range_by_mask(rule->sip, rule->sip_mask, tempfilt->dim[DIM_HI_SIP], tempfilt->dim[DIM_LO_SIP]);	
    }
    else
    {
        read_ip_range_by_start_end_point(rule->start_sip, rule->end_sip, tempfilt->dim[DIM_HI_SIP], tempfilt->dim[DIM_LO_SIP]);
    }
    /* dip */
    if(rule->dip_mask != RFC_INVALID_IP_MASK_LEN)
    {
        read_ip_range_by_mask(rule->dip, rule->dip_mask, tempfilt->dim[DIM_HI_DIP], tempfilt->dim[DIM_LO_DIP]);	
    }
    else
    {
        read_ip_range_by_start_end_point(rule->start_dip, rule->end_dip, tempfilt->dim[DIM_HI_DIP], tempfilt->dim[DIM_LO_DIP]);
    }
    /* sport */
    tempfilt->dim[DIM_SPORT][EP_START] = rule->start_sport;    
    tempfilt->dim[DIM_SPORT][EP_END] = rule->end_sport;    

    /* dport */
    tempfilt->dim[DIM_DPORT][EP_START] = rule->start_dport;    
    tempfilt->dim[DIM_DPORT][EP_END] = rule->end_dport;    

    /* proto */
    tempfilt->dim[DIM_PROTO][EP_START] = rule->start_proto;    
    tempfilt->dim[DIM_PROTO][EP_END] = rule->end_proto;    

    /* vlan */
    tempfilt->dim[DIM_VID][EP_START] = rule->start_vid;    
    tempfilt->dim[DIM_VID][EP_END] = rule->end_vid;    

    /* cost */
    tempfilt->acl_rule_id = rule_id;

    memcpy(&(ip_filtset->filt_arr[ip_filtset->n_filts]), tempfilt, sizeof(filter_t));
		
    ip_filtset->n_filts++;	   
}

/*******************************************************************************
  @Function Name:   rfc_set_phase0_dim_cell
  @Description:     encode different component's phase0 node from filtset 
  @param:  node     point to node which ready to encode
  @return:      
********************************************************************************/
int rfc_set_phase0_dim_cell(vlib_rfc_main_t *rm, pnode_t *node, uint32_t com)
{
    uint32_t i, j, n;
    uint32_t bmp[SIZE];
    int32_t eqid = -1;
    uint32_t rule_num = 0;
    filt_end_point_t rule_arr[2*MAXFILTERS];
    filt_end_point_t rule;
    list_eq_t *p_eqs = NULL;

    if (node == NULL)
    {
        return -1;
    }

    pnode_eqs  = &rm->pnode_eqs[0];
    pnoder_eqs = &rm->pnoder_eqs[0];
    pnode_map_eqs = &pnode_eqs[DIM_V6_P0_NIP_MAX];

    rule_num = ipv4_filtset.n_filts;

    for (j = 0; j < SIZE; j++) {
        bmp[j] = 0;
    }


    p_eqs = &pnode_eqs[com];
    p_eqs->n_ces = 0;
    
    /* ready for sort */
    memset(rule_arr, 0, 2*sizeof(filt_end_point_t)*MAXFILTERS);
    for (i = 0; i < rule_num; i++)
    {   
	rule_arr[ 2*i ].value = ipv4_filtset.filt_arr[i].dim[com][0];
	rule_arr[ 2*i ].end_flag = 1;
        rule_arr[ 2*i ].filt_id = i;
        if(ipv4_filtset.filt_arr[i].dim[com][1] != (uint32_t)-1 )
        {
            rule_arr[ 2*i+1 ].value = ipv4_filtset.filt_arr[i].dim[com][1]+1;
            rule_arr[ 2*i+1 ].end_flag = -1; 
        }
        else
        {
            rule_arr[ 2*i+1 ].value = ipv4_filtset.filt_arr[i].dim[com][1];
            rule_arr[ 2*i+1 ].end_flag = -2; 
        }
        rule_arr[ 2*i+1 ].filt_id = i;
    }

    /* sort end point */
    for (i = 0; i < 2*rule_num-1; i++)
    {
        for (j = i+1; j < 2*rule_num; j++)
        {
            if (rule_arr[i].value > rule_arr[j].value)
		    {
				memcpy(&rule, &rule_arr[i], sizeof(filt_end_point_t));
				memcpy(&rule_arr[i], &rule_arr[j], sizeof(filt_end_point_t));
				memcpy(&rule_arr[j], &rule, sizeof(filt_end_point_t));
		    }
        }
    }
    
    /* encode cell
     *
     * cellnum scope in 0-65535 ,and rule's end point hash in 0-65535.
     * rules of the cellnum matched changes when value equals rules's 
     * end point, so encode cell only traversal once cellnum in 0-65535 
     * and rules's end point. 
     */
    for (i = 0, n = 0; n < RFC_DIM_VALUE + 1 && i <= 2 * rule_num;) {
        if (n < rule_arr[i].value || i == 2*rule_num)
	    {
            if (-1 == eqid)
            {
                if (p_eqs->n_ces == EQS_SIZE)
                {
                    printf("ERROR : com %d number of rfc node ces is full!\r\n", com);
                    return -1;
                }
                eqid = add_list_eqs_ces(p_eqs, bmp);
            }
            node->cell[n] = eqid;
            n++;

		}
		else if (n == rule_arr[i].value)
		{
		    if (rule_arr[i].end_flag == 1)
	            {
	                bmpset(bmp, rule_arr[i].filt_id, TRUE);
	            }
		    else if (rule_arr[i].end_flag == -1)
	            {
	                bmpset(bmp, rule_arr[i].filt_id, FALSE);
	            }
			else // if (rule_arr[i].end_flag == -2)
			{
				//don't change rule pool
			}
		    if ((i < 2*rule_num-1 && rule_arr[i].value < rule_arr[i+1].value) || (i == 2*rule_num-1))
		    {
			eqid = bmpquery(p_eqs, bmp);
			if (-1 == eqid)
			{
			    if (p_eqs->n_ces == EQS_SIZE)
			    {
			        printf("ERROR : com %d number of rfc node ces is full!\r\n", com);
			        return -1;
			    }

			    eqid = add_list_eqs_ces(p_eqs, bmp);
			}
			node->cell[n] = eqid;
			n++;
		    }

		    i++;
		}
		else if (n > rule_arr[i].value)
		{
		    i++;
		}

     }//for i n

    node->n_ces = p_eqs->n_ces; 
    return 0;
}

/*******************************************************************************
   @Function Name:   rfc_set_phase1_dim_cell
   @Description:     encode phase1 node from phase 0, must called after 
		     rfc_set_phase0_dim_cell
   @param:  nodeN    point to phase0 node
   @param:  noder    point to phase1 node which ready to encode
   @return:      
********************************************************************************/
int rfc_set_phase1_dim_cell(pnoder_t *noder, uint8_t com, uint8_t ip_ver)
{
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t k = 0;
    uint32_t m = 0;
    uint32_t w = 0;
    uint32_t indx = 0;
    list_eq_t *p_eqs;
    ce_t ces_one;
    ce_t *ces1 = &ces_one;
    ce_t *ces2 = &ces_one;
    ce_t *ces3 = &ces_one;
    ce_t *ces4 = &ces_one;
    uint32_t n_ces1 = 1;
    uint32_t n_ces2 = 1;
    uint32_t n_ces3 = 1;
    uint32_t n_ces4 = 1;
    int32_t eqid;
    uint32_t mid_bmp[SIZE];
    uint8_t valid_node = 0;
    
    p_eqs = &pnoder_eqs[com];
    p_eqs->n_ces = 0;

    if (noder == NULL || com > 1 || (ip_ver != 4 && ip_ver != 6))
	return -2;

    ces_one.eqid = 0;

    for (i = 0; i < SIZE; i++)
    {
        ces_one.cbm[i] = RFC_UINT32_ALL_BIT_SET;
    }

    if (ip_ver == 4 && com == 0)
    {
        valid_node = 4; 
        n_ces1 = pnode_eqs[0].n_ces;
        n_ces2 = pnode_eqs[1].n_ces;
        n_ces3 = pnode_eqs[2].n_ces;
        n_ces4 = pnode_eqs[3].n_ces;
        ces1 = &pnode_eqs[0].ces[0];
        ces2 = &pnode_eqs[1].ces[0];
        ces3 = &pnode_eqs[2].ces[0];
        ces4 = &pnode_eqs[3].ces[0];
    }
    else if (ip_ver == 4 && com == 1)
    {
        valid_node = 4;
        n_ces1 = pnode_eqs[4].n_ces;
        n_ces2 = pnode_eqs[5].n_ces;
        n_ces3 = pnode_eqs[6].n_ces;
        n_ces4 = pnode_eqs[7].n_ces;
        ces1 = &pnode_eqs[4].ces[0];
        ces2 = &pnode_eqs[5].ces[0];
        ces3 = &pnode_eqs[6].ces[0];
        ces4 = &pnode_eqs[7].ces[0];
    }
    else if (ip_ver == 6 && com == 0)
    {
        valid_node = 3;
        n_ces1 = pnode_map_eqs[0].n_ces;
        n_ces2 = pnode_map_eqs[1].n_ces;
        n_ces3 = pnode_eqs[3].n_ces;
        ces1 = &pnode_map_eqs[0].ces[0];
        ces2 = &pnode_map_eqs[1].ces[0];
        ces3 = &pnode_eqs[3].ces[0];
    }
    else if (ip_ver == 6 && com == 1)
    {
        valid_node = 3;
        n_ces1 = pnode_eqs[0].n_ces;
        n_ces2 = pnode_eqs[1].n_ces;
        n_ces3 = pnode_eqs[2].n_ces;
        ces1 = &pnode_eqs[0].ces[0];
        ces2 = &pnode_eqs[1].ces[0];
        ces3 = &pnode_eqs[2].ces[0];
    }
    else
    {
        return -1;
    }

    noder->ncells = n_ces1 * n_ces2 * n_ces3 * n_ces4;
    if (noder->ncells > P1_CELL_ENTRY_SIZE)
    {
	printf("ERROR:phase 1 cells number %d is biger then %d\r\n", noder->ncells, P1_CELL_ENTRY_SIZE);
	return -1;
    }
    
    for (i = 0; i < n_ces1; i++)
    {
        for (j = 0; j < n_ces2; j++)
        {
            for (k = 0; k < n_ces3; k++)
            {
                for (w =0; w < n_ces4; w++)
                {
                    for (m = 0; m < SIZE; m++)
                    {
                        mid_bmp[m] = ces1[i].cbm[m] & ces2[j].cbm[m] & ces3[k].cbm[m] & ces4[w].cbm[m];
                    }

                    eqid = bmpquery(p_eqs, mid_bmp);
                    if (-1 == eqid)
                    {
                        if (p_eqs->n_ces >= EQS_SIZE)
                        {
                            goto cell_full_error;
                        }

                        eqid = add_list_eqs_ces(p_eqs, mid_bmp);
                    }

                    noder->cell[indx] = eqid;
                    indx++;
                }
            }
        }
    }
    
    noder->n_ces = p_eqs->n_ces;
    return 0;

cell_full_error:

    printf("ERROR : rfc phase1 node ces number is full!\r\n");
    return -1;
}

/*******************************************************************************
   @Function Name:   rfc_set_phase2_cell
   @Description:     encode phase2 node from phase 1, must called after 
		     rfc_set_phase1_dim_cell
   @param:  nodeN    point to phase1 node
   @param:  noder    point to phase2 node which ready to encode
   @return:      
********************************************************************************/
int rfc_set_phase2_cell(pnoder_t *noder)
{
    uint32_t indx = 0;
    unsigned short rule_indx = 0;
    ce_t *ces1, *ces2;
    uint32_t n_ces1, n_ces2;
    unsigned int end_bmp[SIZE];
    uint32_t i, j, m;

    /* Initialize noder */
    noder->n_ces = 0;
    n_ces1 = pnoder_eqs[0].n_ces;
    n_ces2 = pnoder_eqs[1].n_ces;
    ces1 = &pnoder_eqs[0].ces[0];
    ces2 = &pnoder_eqs[1].ces[0];
    
    noder->ncells = n_ces1 * n_ces2;
    if (noder->ncells > P2_CELL_ENTRY_SIZE) {
        printf("ERROR:phase 2 cells number %d is biger then %d\r\n", noder->ncells, P2_CELL_ENTRY_SIZE);
		return -1;
    }

    for (i = 0; i < n_ces1; i++)
    {
		for (j = 0; j < n_ces2; j++) {
		    /* generate end_bmp */
		    for (m = 0; m < SIZE; m++) {
                end_bmp[m] = ces1[i].cbm[m] & ces2[j].cbm[m];
            }

		    /* get rule indx with highest priority */
		    rule_indx = get_high_pri_rule_indx(end_bmp);

		    /* Set Cell bits */
		    noder->cell[indx] = rule_indx;
		    indx++;
		}
    }

    return 0;
}
#endif
