# OSaSP_Lab4
Pavel Bobryk 051004;  
List 4;  
## Task 1: 01.c:  
* 0 -> (1, 2)  
* 0 -> (1, 2) SIGUSR1  
* (1, 2) -> 0 SIGUSR2
## Task: Lab4_v4_tree.c; Lab4_v4_all_task.c
### Lab4_v4_tree.c
Tree:  
* 0 -> 1
* 1 -> (2, 3, 4, 5)
* 5 -> (6, 7, 8)
### Lab4_v4_all_task.c
Same tree as Lab4_v4_tree.c  
1) 1 -> (2, 3, 4, 5) SIGUSR1
2) 5 -> (6, 7, 8) SIGUSR1
3) 8 -> 1 SIGUSR1
