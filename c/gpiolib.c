#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>

#ifdef CTL
#include <getopt.h>
#endif
#include "gpiolib.h"

int gpio_direction(int gpio, int dir)
{
	int ret = 0;
	char buf[50];
	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
	int gpiofd = open(buf, O_WRONLY);
	if(gpiofd < 0) {
#ifdef CTL
		perror("Couldn't open IRQ file");
#endif
		ret = -1;
	}

	if(dir == 1 && gpiofd){
		if (3 != write(gpiofd, "out", 3)) {
#ifdef CTL
			perror("Couldn't set GPIO direction to out");
#endif
			ret = -2;
		}
	}
	else if(gpiofd) {
		if(2 != write(gpiofd, "in", 2)) {
#ifdef CTL
			perror("Couldn't set GPIO directio to in");
#endif
			ret = -3;
		}
	}

	close(gpiofd);
	return ret;
}

int gpio_setedge(int gpio, int rising, int falling)
{
	int ret = 0;
	char buf[50];
	sprintf(buf, "/sys/class/gpio/gpio%d/edge", gpio);
	int gpiofd = open(buf, O_WRONLY);
	if(gpiofd < 0) {
#ifdef CTL
		perror("Couldn't open IRQ file");
#endif
		ret = -1;
	}

	if(gpiofd && rising && falling) {
		if(4 != write(gpiofd, "both", 4)) {
#ifdef CTL
			perror("Failed to set IRQ to both falling & rising");
#endif
			ret = -2;
		}
	} else {
		if(rising && gpiofd) {
			if(6 != write(gpiofd, "rising", 6)) {
#ifdef CTL
				perror("Failed to set IRQ to rising");
#endif
				ret = -2;
			}
		} else if(falling && gpiofd) {
			if(7 != write(gpiofd, "falling", 7)) {
#ifdef CTL
				perror("Failed to set IRQ to falling");
#endif
				ret = -3;
			}
		}
	}

	close(gpiofd);
	return ret;
}

int gpio_select(int gpio)
{
	char gpio_irq[64];
	int ret = 0, buf, irqfd;
	fd_set fds;
	FD_ZERO(&fds);

	snprintf(gpio_irq, sizeof(gpio_irq), "/sys/class/gpio/gpio%d/value", gpio);
	irqfd = open(gpio_irq, O_RDONLY, S_IREAD);
	if(irqfd < 1) {
#ifdef CTL
		perror("Couldn't open the value file");
#endif
		return -1;
	}

	// Read first since there is always an initial status
	read(irqfd, &buf, sizeof(buf));

	while(1) {
		FD_SET(irqfd, &fds);
		ret = select(irqfd + 1, NULL, NULL, &fds, NULL);
		if(FD_ISSET(irqfd, &fds))
		{
			FD_CLR(irqfd, &fds);  //Remove the filedes from set
			// Clear the junk data in the IRQ file
			read(irqfd, &buf, sizeof(buf));
			return 1;
		}
	}
}

int gpio_export(int gpio)
{
	int efd;
	char buf[50];
	int ret;
	efd = open("/sys/class/gpio/export", O_WRONLY);

	if(efd != -1) {
		sprintf(buf, "%d", gpio); 
		ret = write(efd, buf, strlen(buf));
		if(ret < 0) {
#ifdef CTL
			perror("Export failed");
#endif
			return -2;
		}
		close(efd);
	} else {
		// If we can't open the export file, we probably
		// dont have any gpio permissions
		return -1;
	}
	return 0;
}

void gpio_unexport(int gpio)
{
	int gpiofd;
	char buf[50];
	gpiofd = open("/sys/class/gpio/unexport", O_WRONLY);
	sprintf(buf, "%d", gpio);
	write(gpiofd, buf, strlen(buf));
	close(gpiofd);
}

int gpio_read(int gpio)
{
	char in[3] = {0, 0, 0};
	char buf[50];
	int nread, gpiofd;
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	gpiofd = open(buf, O_RDWR);
	if(gpiofd < 0) {
#ifdef CTL
		fprintf(stderr, "Failed to open gpio %d value\n", gpio);
		perror("gpio failed");
#endif
	}

	do {
		nread = read(gpiofd, in, 1);
	} while (nread == 0);
	if(nread == -1){
#ifdef CTL
		perror("GPIO Read failed");
#endif
		return -1;
	}
	
	close(gpiofd);
	return atoi(in);
}

int gpio_write(int gpio, int val)
{	
	char buf[50];
	int ret, gpiofd;
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	gpiofd = open(buf, O_RDWR);
	if(gpiofd > 0) {
		snprintf(buf, 2, "%d", val);
		ret = write(gpiofd, buf, 2);
		if(ret < 0) {
#ifdef CTL
			perror("failed to set gpio");
#endif
			return 1;
		}

		close(gpiofd);
		if(ret == 2) return 0;
	}
	return 1;
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Simple gpio access\n"
	  "\n"
	  "  -h, --help             This message\n"
	  "  -p, --getin <dio>      Returns the input value of n sysfs DIO\n"
	  "  -e, --setout <dio>     Sets a sysfs DIO output value high\n"
	  "  -l, --clrout <dio>     Sets a sysfs DIO output value low\n"
	  "  -d, --ddrout <dio>     Set sysfs DIO to an output\n"
	  "  -r, --ddrin <dio>      Set sysfs DIO to an input\n\n",
	  argv[0]
	);
}

int main(int argc, char **argv)
{
	int c;
	static struct option long_options[] = {
	  { "getin", 1, 0, 'p' },
	  { "setout", 1, 0, 'e' },
	  { "clrout", 1, 0, 'l' },
	  { "ddrout", 1, 0, 'd' },
	  { "ddrin", 1, 0, 'r' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	if(argc == 1) {
		usage(argv);
		return(1);
	}

	while((c = getopt_long(argc, argv, "p:e:l:d:r:", long_options, NULL)) != -1) {
		int gpio, i;

		switch(c) {
		case 'p':
			gpio = atoi(optarg);
			gpio_export(gpio);
			printf("gpio%d=%d\n", gpio, gpio_read(gpio));
			gpio_unexport(gpio);
			break;
		case 'e':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_write(gpio, 1);
			gpio_unexport(gpio);
			break;
		case 'l':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_write(gpio, 0);
			gpio_unexport(gpio);
			break;
		case 'd':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_direction(gpio, 1);
			gpio_unexport(gpio);
			break;
		case 'r':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_direction(gpio, 0);
			gpio_unexport(gpio);
			break;
		case 'h':
		default:
			usage(argv);
		}
	}
}

#endif // CTL
