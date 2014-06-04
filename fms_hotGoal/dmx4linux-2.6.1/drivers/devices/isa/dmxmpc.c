/*
 *
 * Martin Proscenium ISA card
 *
 */

/* Register Map:
 * 0x801-0x80a  The string "DMXPLUS   ". Can probably be used to autodetect the card.
 * 0x80d        Bit 0 enables the transmitter. Bit 1 enables the receiver.
 * 0x814-0x815  Counts when transmitter enabled, not sure what it counts.
 * 0x81c-0x81d  16-bit counter (big endian) containing the size of the received block.
 * 0x820-0x821  Counts when receiver enabled.
 * 0xa00-0xbff  Transmit buffer
 * 0xc00-0xdff  Receive buffer
 */

#define MPC_TRANSMITTER (0)
#define MPC_RECEIVER    (1)

/*
 * ======== Card Hardware Abstraction Layer (HAL)
 */


unsigned char mpc_hal_readregister (dmxmpc_t *mpc, int reg)
{
  if (reg >= 0x800 && reg < 0x1000)
    return ((unsigned char volatile *)mpc->ptr)[reg];
  return 0;
}

void mpc_hal_writeregister (dmxmpc_t *mpc, int reg, unsigned char value)
{
  if (reg >= 0x800 && reg < 0x1000)
    ((unsigned char volatile *)mpc->ptr)[reg] = value;
}



void mpc_hal_frobregister (dmxmpc_t *mpc, int reg, unsigned char mask, unsigned char value)
{
  return mpc_hal_writeregister (mpc, reg, (mpc_hal_readregister(mpc, reg) & ~mask) | (value & mask));
}


/*
 * ======== Card Abstraction Layer
 */


char  dmxmpc_has_signature (dmxmpc_t *mpc)
{
  if (mpc)
    {
      unsigned char sig[11];
      char i;
      for (i=0; i<10; i++)
	sig[i] = mpc_hal_readregister (mpc, 0x801+i);
      sig[10]='\0';
      return (strcmp (sig, "DMXPLUS   ")==0);
    }
  // 0x801-0x80a  The string "DMXPLUS   ". Can probably be used to autodetect the card.
  return 0; // false
}

/*
 * port = 0 => Transmitter
 * port = 1 => Receiver
 */
void dmxmpc_enable_port (dmxmpc_t *mpc, char port, char on)
{
  if (mpc)
    {
      //* 0x80d  Bit 0 enables the transmitter. Bit 1 enables the receiver.
      mpc_hal_frobregister (mpc, 0x80d, (1<<port), on ? (1<<port) : 0);
    }
}

unsigned long dmxmpc_txcount (dmxmpc_t *mpc)
{
  return (mpc_hal_readregister (mpc, 0x814)<<8) | mpc_hal_readregister (mpc, 0x815);
}

unsigned long dmxmpc_rxsize (dmxmpc_t *mpc)
{
  return (mpc_hal_readregister (mpc, 0x81c)<<8) | mpc_hal_readregister (mpc, 0x81d);
}


unsigned long dmxmpc_rxcount (dmxmpc_t *mpc)
{
  return (mpc_hal_readregister (mpc, 0x820)<<8) | mpc_hal_readregister (mpc, 0x821);
}


/*
 * port = 0 => Transmitter
 * port = 1 => Receiver
 */
void dmxmpc_setchannel (dmxmpc_t *mpc, char port, int channel, unsigned char value)
{
  if (channel >= 0 && channel < 512 && port >= 0 && port < 2)
    mpc_hal_writeregister (mpc, 0xa00+(port*0x200)+channel, value);
}


/*
 * port = 0 => Transmitter
 * port = 1 => Receiver
 */
unsigned char dmxmpc_getchannel (dmxmpc_t *mpc, char port, int channel)
{
  if (channel >= 0 && channel < 512 && port >= 0 && port < 2)
    return mpc_hal_readregister (mpc, 0xa00+(port*0x200)+channel);
  return 0;
}




initialize (unsigned long address)
{
  dmxmpc_t  mpc;
  mpc.physaddr = address;
  mpc.ptr = ioremap (mpc.physaddr, ...);

  if (dmxmpc_has_signature (&mpc))
    {
      // allocate mpc dynamicly and assign settings to it;
      newmpc = .....;

      dmxmpc_enable_port (newmpc, MPC_TRANSMITTER, 1);
      dmxmpc_enable_port (newmpc, MPC_RECEIVER, 1);
    }
}


writedmxout (unsigned char *buffer, int size)
{
  int i;
  if (size > 512)
    size = 512;
  for (i=0; i<size; i++)
    dmxmpc_setchannel (newmpc, MPC_TRANSMITTER, i, buffer[i]);
}

readdmxin (unsigned char *buffer, int size)
{
  int i;
  if (size > 512)
    size = 512;
  for (i=0; i<size; i++)
  buffer[i] = dmxmpc_getchannel (newmpc, MPC_RECEIVER, i);
}

