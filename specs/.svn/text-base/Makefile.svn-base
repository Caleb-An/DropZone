all: final_paper

final_paper: final_paper.txt
	pandoc -c azul.css final_paper.txt -o final_paper.html

clean:
	rm -f final_paper.html
