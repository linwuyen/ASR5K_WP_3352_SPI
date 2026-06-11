/*
 * c28protection.h
 *
 *  Created on: May 8, 2024
 *      Author: User
 */

#ifndef C28PROTECTION_H_
#define C28PROTECTION_H_

static inline void scanProtection(void)
{

}

static inline void scanWarning(void)
{


}

static inline void rstError(void)
{
    RST_ERR((_ALL_ERROR|_ALL_WARNING), sDrv);
}


#endif /* C28PROTECTION_H_ */
