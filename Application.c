/* 
Aplikacija : Napraviti jednostavnu C aplikaciju za rad sa drajverom.
Aplikacija izlistava “meni” sa mogućnostima navedenim ispod, te prepušta
korisniku da izabere željenu opciju tako što unese odgovarajući broj. Aplikacija
za svaku od opcija 1-7 traži od korisnika da unese dodatne podatke ukoliko su
neophodni (npr. koji novi string da se upiše, koji string da se konkatanira, koji
izraz da se izbriše iz stringa i koliko poslednjih karaktera da se izbriše), a zatim
vrši tu operaciju nad drajverom. Pri svakoj izvršenoj operaciji se “meni” ponovo
izlistava. Tek kada korisnik unese karakter ‘Q’ umesto broja, aplikacija se
terminira.
1: Pročitaj trenutno stanje stringa
2: Upiši novi string
3: Konkataniraj string na trenutni
4: Izbriši čitav string
5: Izbriši vodeće i prateće space karaktere
6: Izbriši izraz iz stringa
7: Izbriši poslednjih n karaktera iz stringa
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define STR_SIZE 101

int main(int argc, char** argv)
{
	FILE* fp;
	size_t option_bytes = 2;
	size_t str_bytes = STR_SIZE;
	char *option = (char*)malloc(option_bytes);
	char path[] = "/dev/stred";
	int i = 0;
	char *str;
	unsigned char flag = 0;
	
	while(1)
	{
		
		printf("Meni:\n");
		printf("1: Procitaj trenutno stanje stringa\n");
		printf("2: Upisi novi string\n");
		printf("3: Konkataniraj string na trenutni\n");
		printf("4: Izbrisi citav string\n");
		printf("5: Izbrisi vodece i pratece space karaktere\n");
		printf("6: Izbrisi izraz iz stringa\n");
		printf("7: Izbrisi poslednjih n karaktera iz stringa\n");
		
		printf("Choose an option:\n");
		getline(&option, &option_bytes, stdin);
		
		switch(*option - 48)
		{
			case 1:
			
				fp = fopen(path, "r");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				str = (char*)malloc(str_bytes);
				
				// inicijalizacija kako bi se osigurao da je nula na kraju
				for(i = 0; i < str_bytes; i++)
					str[i] = 0;
					
				getline(&str, &str_bytes, fp);
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				
				printf("Current string is %s\n", str);
				free(str);
				
				break;
				
			case 2:
				
				str = (char*)malloc(str_bytes + 1);
				printf("Enter a string\n");
				getline(&str, &str_bytes, stdin);
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fprintf(fp, "string=%s\n", str); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				free(str);
				
				break;
				
			case 3:
			
				str = (char*)malloc(str_bytes + 1);
				printf("Enter a string\n");
				getline(&str, &str_bytes, stdin);
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fprintf(fp, "a`ppend=%s\n", str); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				free(str);
			
				break;
				
			case 4:
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fputs("clear\n", fp); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
			
				break;
				
			case 5:
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fputs("shrink\n", fp); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				
				break;
				
			case 6:
				
				str = (char*)malloc(str_bytes + 1);
				printf("Enter a string\n");
				getline(&str, &str_bytes, stdin);
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fprintf(fp, "remove=%s\n", str); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				free(str);
				
				break;
				
			case 7:
				
				printf("Enter how many characters do you want to truncate\n");
				getline(&option, &option_bytes, stdin);
				
				fp = fopen(path, "w");
				if(fp == NULL)
				{
					printf("Error while opening %s\n", path);
					return -1;
				}
				
				fprintf(fp, "truncate=%s\n", option); // mozda nova linija ne treba
				
				if(fclose(fp))
				{
					printf("Error while closing %s\n", path);
					return -1;
				}
				free(str);
				break;
				
			case 'Q' - 48:
			
				flag = 1;
				break;
			
			default:
				
				printf("Wrong format\n");
			
		}
		
		if(flag)
		{
			free(option);
			break;
		}
		
		
	}
	
	return 0;
}
