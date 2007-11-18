#ifndef _LINUX_TTY_FLIP_H
#define _LINUX_TTY_FLIP_H

extern int tty_buffer_request_room(struct tty_struct *tty, size_t size);
extern int tty_insert_flip_string(struct tty_struct *tty, const unsigned char *chars, size_t size);
extern int tty_insert_flip_string_flags(struct tty_struct *tty, const unsigned char *chars, const char *flags, size_t size);
extern int tty_prepare_flip_string(struct tty_struct *tty, unsigned char **chars, size_t size);
extern int tty_prepare_flip_string_flags(struct tty_struct *tty, unsigned char **chars, char **flags, size_t size);
void tty_schedule_flip(struct tty_struct *tty);

int tty_insert_flip_char(struct tty_struct *tty,
			 unsigned char ch, char flag);

#endif /* _LINUX_TTY_FLIP_H */
