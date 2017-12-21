
# mp1.S - missile-command version
# Good luck, and have fun!

.data
	
    # Constants for accessing the fields of a struct missile, 
    # struct missile is defined in rtc.h and mp1.h

	NEXT 	  = 0
	X	  = 4
	Y	  = 8
	VX	  = 12
	VY	  = 16
	DEST_X	  = 20
	DEST_Y	  = 24
	EXPLODED  = 28
	C	  = 32

    # Character to draw for an explosion - '@'
	EXPLOSION = 64
	CROSSHAIRS_ASCII = 43
	SPACE = 32


# Data shared between this file and rtc.c helper functions

# This '.globl' directive makes these symbols visible externally
.globl mp1_missile_list, base_alive, mp1_score

mp1_missile_list:	.long 0x0	# Head of list pointer
base_alive:		.long 0x0	# Array of 3 bytes, plus a padding byte
mp1_score:		.long 0x0	# Player's current score

# Data private to this  file

base_pic:     	    .string "/^^^\\"	# Picture of a live base
dead_base_pic: 	    .string "xxxxx"	# Picture of a dead base
crosshairs_x:		.long 0x0	# X-position of the crosshairs
crosshairs_y:		.long 0x0	# Y-position of the crosshairs

# data defined by myself
base1_offset:		.long 3876	# (24*80 + 18)*2
base2_offset:		.long 3916	# (24*80 + 38)*2
base3_offset:		.long 3956  # (24*80 + 58)*2
address_data_copy_to: .long 0
test_list_long:		.long 0		# for test

.text

# void mp1_poke(void);
# You can use this function to write to video memory.
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX

mp1_poke:
		movl    vmem_base_addr(,1),%edx
		movb    %cl,(%edx,%eax,1)
		ret

#   ----------------- Exported functions ---------------------

# void mp1_rtc_tasklet(unsigned long garbage);
# Performs three tasks:
#	(1) update the missiles
#	(2) update the cities
#	(3) update the crosshairs
# %edi as the address of current list of missile	    -----#
# %esi as the address of former list of missile		    -----#
# Registers: Standard C calling convention

.globl mp1_rtc_tasklet
mp1_rtc_tasklet:
# when "mp1_rtc_tasklet" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	#test
	# clean screen
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			call 	clean_all_screen
		popl	%edx
		popl	%ecx
		popl	%eax
	#end test
			
	#(1) call "update_missiles" to update the missiles
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			call	update_missiles
		popl	%edx
		popl	%ecx
		popl	%eax
	
	#(2) call "update_cities" to update the cities
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			call	update_cities
		popl	%edx
		popl	%ecx
		popl	%eax
		
	#(3) call "update_crosshairs" to update the crosshairs
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			call	update_crosshairs
		popl	%edx
		popl	%ecx
		popl	%eax
		
# "mp1_rtc_tasklet" is done. Restore everything
done_mp1_rtc_tasklet:
	leal	-12(,%ebp,), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl(unsigned long arg, unsigned int cmd)
# The dispatch function for the MP1 ioctls - should use the cmd argument
# and a jumptable to execute one of the specific ioctls implemented below.
# Inputs   : unsigned long arg - parameter to the mp1_ioctl_....
#          : unsigned int cmd  - specifies which mp1_ioctl_... function 
#          :                     to execute
# Outputs  : Returns an integer - depends on which ioctl() function is called
# Registers: Standard C calling convention
.globl mp1_ioctl
mp1_ioctl:
	# retrieve the parameters
	movl	8(%esp), %eax		#cmd
	
	# check cmd's value to decide where to jump
		## if cmd < 0 or cmd > 4, jump to "mp1_ioctl_fail"
		cmpl	$0, %eax
		jl		mp1_ioctl_fail
		cmpl	$4, %eax
		jg		mp1_ioctl_fail
		
		## else if cmd = 1, 2, 3 or 4
		jmp		*jump_table(,%eax,4)	# 4 bytes
	
		## if cmd != 0, 1, 2, 3 or 4, return -1
		mp1_ioctl_fail:
			xorl	%eax, %eax
			subl	$1, %eax
			ret

# jump_table
jump_table:	.long mp1_ioctl_startgame, mp1_ioctl_addmissile, mp1_ioctl_movexhairs, mp1_ioctl_getstatus, mp1_ioctl_endgame
		

# ----------------- Functions private to this file -------------------

# void update_missiles(void)
# (1) check if the missile is still inside the screen
# (2) check if the missile has reached its destination
# (3) check if the missile is exploding
# Registers: Standard C calling convention
# %edi used as the address of current list of missile  		  -----#
# %esi used as the address of former list of missile          -----#
update_missiles:
# when "update_missiles" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
# assign the address of the head list into %edi
	movl 	mp1_missile_list, %edi
# assign the address of the head list into %esi (initiailization)
	movl 	%edi, %esi

# check if the current missile exists?	
update_missiles_check_if_exist:
	cmpl	$0x0, %edi
	## if no then finishing this function
	je		done_update_missiles	
	
# (1) update_missiles_part1_check_missile_screen
# check if the missile is still inside the screen
	update_missiles_part1_check_missile_screen:
		# check if the missile x location < 0
			movl	X(%edi), %eax
			shrl	$16, %eax
			cmpl	$0, %eax
			jl		missile_out_of_screen
		# check if the missile x location > 79 
			movl	X(%edi), %eax
			shrl	$16, %eax
			cmpl	$79, %eax 
			jg		missile_out_of_screen
		# check if the missile y location < 0
			movl	Y(%edi), %eax
			shrl	$16, %eax
			cmpl	$0, %eax 
			jl		missile_out_of_screen
		# check if the missile y location > 24
			movl	Y(%edi), %eax
			shrl	$16, %eax
			cmpl	$24, %eax
			jg		missile_out_of_screen
			
		## if the missile is inside the screen
			jmp		update_missiles_part2_check_missile_destination
		## if the missile is out of the screen, free this missile
		missile_out_of_screen:
			pushl	%eax
			pushl	%ecx
			pushl	%edx
				pushl 	%edi							#parameter: the address of the current missile which need to be removed	
				pushl	%esi							#parameter: the address of the former missile
				call	mp1_missile_remove
				decl	test_list_long					#test
				addl	$8, %esp					
			popl	%edx
			popl	%ecx
			popl	%eax
			jmp		check_next_missile_if_remove

# (2) update_missiles_part2_check_missile_destination
# check if the missile has reached its destination
	update_missiles_part2_check_missile_destination:
		# check x-axis_location and dest_X
				movl	X(%edi), %eax
				shrl	$16, %eax
				cmpl	DEST_X(%edi), %eax
			## if the missile has not reached the destination
			jne		update_missiles_part3_check_missiles_exploding
		# check y-axis_location and dest_Y
				movl	Y(%edi), %eax
				shrl	$16, %eax
				cmpl	DEST_Y(%edi), %eax
			## if the missile has not reached the destination
			jne		update_missiles_part3_check_missiles_exploding
			## if the missile has reached the destination
			jmp		update_missiles_call_explode
	
# (3) update_missiles_part3_check_missiles_exploding
# check if the missile is exploding
	update_missiles_part3_check_missiles_exploding:
			cmpl	$0, EXPLODED(%edi)
		## if the missile is not exploding
			je		update_missiles_location
		## if the missile is exploding
			jmp		update_missiles_call_explode
	
# update_missiles_call_explode
# check if the missile is exploded by cities or other missiles (1: yes, 0: no)	
# %ebx is used to get the result of "missile_explode". (1: yes, 0: no)	
	update_missiles_call_explode:
		xorl 	%ebx, %ebx						#initiailize %ebx
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	%edi						#parameter: address_current_missile
			call	missile_explode
			movl	%eax, %ebx					#store the result of "missile_explode" in ebx
			addl	$4, %esp	
		popl	%edx
		popl	%ecx
		popl	%eax
	# check if the result of "missile_explode" is 0
		cmpl	$0, %ebx
	## if the result != 0
		jne		call_mp1_notify_user
	## if the result = 0
		jmp		decrement_after_missile_explode
		
# call_mp1_notify_user
# call "mp1_notify_user" to notify the user		
	call_mp1_notify_user:
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			call	mp1_notify_user
		popl	%edx
		popl	%ecx
		popl	%eax
# decrement_after_missile_explode
# after calling missile_explode, decrement the "EXPLODED" field	
	decrement_after_missile_explode:
		movl 	EXPLODED(%edi), %eax
		decl	%eax
		movl 	%eax, EXPLODED(%edi)
	# check if the "EXPLODED" field is 0
		cmpl	$0, %eax
	## if the "EXPLODED" field != 0
		jne 	draw_missile
	## if the "EXPLODED" field = 0
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl 	%edi							#parameter: the address of the current missile which need to be removed	
			pushl	%esi							#parameter: the address of the former missile
			call	mp1_missile_remove				
			decl	test_list_long					#test
			addl	$8, %esp					
		popl	%edx
		popl	%ecx
		popl	%eax
		jmp		check_next_missile_if_remove

# update_missiles_location
# move the current missile by adding the VX and VY to its X and Y			
	update_missiles_location:
		movl	X(%edi), %eax
		addl	VX(%edi), %eax
		movl	%eax, X(%edi)
		movl	Y(%edi), %eax
		addl	VY(%edi), %eax
		movl	%eax, Y(%edi)

# draw_missile
# since the removed missile won't come here, here will only draw existing or exploding
draw_missile:
	pushl	%eax
	pushl	%ecx
	pushl	%edx
		# calculate the offset
		movl	Y(%edi), %edx
		shrl	$16, %edx
		movl	$80, %eax
		mull	%edx
		movl	%eax, %ecx
		movl	X(%edi), %edx
		shrl	$16, %edx
		addl	%edx, %ecx	
		movl	$2, %eax
		mull	%ecx
		
		# check if the missile is exploding
		cmpl	$0, EXPLODED(%edi)
		## if it is exploding
		jne		draw_exploding_missile
		## if it is not exploding, draw_normal_missile
	draw_normal_missile:
		movb	C(%edi), %cl
		jmp		draw_missile_call_mp1_poke
		
	draw_exploding_missile:
		movb	$EXPLOSION, %cl
		
		# call mp1_poke
	draw_missile_call_mp1_poke:
		call 	mp1_poke	
	popl	%edx
	popl	%ecx
	popl	%eax

# check_next_missile
# after finishing updating the current missile, check if the next missile exists
## if the current has not been removed
	check_next_missile:
		movl	%edi, %esi						#store former address of the missile
		movl	NEXT(%esi), %edi				#put the address of next missile into %edi
		jmp		update_missiles_check_if_exist
## if the current has been removed, the former address remains unchanged
	check_next_missile_if_remove:
		movl	NEXT(%esi), %edi				#put the address of next missile into %edi
		jmp		update_missiles_check_if_exist

# "update_missiles" is done. Restore everything
done_update_missiles:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# void mp1_missile_remove(int address_former_missile, int address_current_missile)
# remove the current missile from the list
# 	inputs: %esi as the address of former list of missile
# 			%edi as the address of current list of missile
# Registers: Standard C calling convention
mp1_missile_remove:
# when "mp1_missile_remove" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi	
# retrieve the parameter
	movl	8(%ebp), %esi			# esi: address of the former missile
	movl	12(%ebp), %edi			# edi: address of the current missile
	
	# check if the current missile is in the head of the list
		cmpl	mp1_missile_list, %edi
		je		mp1_missile_remove_head
	## if the missile is not in the head of the list, the former missile should connect to next missile
			movl	NEXT(%edi), %eax
			movl	%eax, NEXT(%esi)
		jmp		mp1_missile_remove_call_mp1_free
	## if the missile is in the head of the list, mp1_missile_list = the address of the next missile
	mp1_missile_remove_head:
			movl	NEXT(%edi), %eax 
			movl	%eax, mp1_missile_list
		jmp		mp1_missile_remove_call_mp1_free

# after changing the connection, the current missile should be free
	mp1_missile_remove_call_mp1_free:
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl 	%edi							#parameter: the address of the current missile which need to be removed
			call	mp1_free						
			addl	$4, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
	
# "mp1_missile_remove" is done. Restore everything
done_mp1_missile_remove:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# void update_cities(void)
# update the status of the cities and draw it
# Registers: Standard C calling convention
# %edi used as status (alive or dead) of base 1  		  -----#
# %esi used as status (alive or dead) of base 2  		  -----#
# %ebx used as status (alive or dead) of base 3  		  -----#
update_cities:
# when "update_cities" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# check if base 1 is alive
	check_base_1:
		# initiailize %edi to 1 (alive)
		movl	$1, %edi
		cmpb	$0, base_alive
		## if != 0, base 1 is alive
		jne		update_cities_draw_base1_call
		## if = 0, base 1 is dead. %edi = 0
		xorl	%edi, %edi
		# call "update_cities_draw_base1_call" function to draw base 1
		update_cities_draw_base1_call:
			pushl	%eax
			pushl	%ecx
			pushl	%edx
				pushl 	%edi						
				pushl	base1_offset
				call	update_cities_draw				
				addl	$8, %esp					
			popl	%edx
			popl	%ecx
			popl	%eax
			
	# check if base 2 is alive
	check_base_2:
		# initiailize %esi to 1 (alive)
		movl	$1, %esi
		movb	base_alive+1, %al
		cmpb	$0, %al
		## if != 0, base 2 is alive
		jne		update_cities_draw_base2_call
		## if = 0, base 2 is dead. %esi = 0
		xorl	%esi, %esi
		# call "update_cities_draw_base2_call" function to draw base 2
		update_cities_draw_base2_call:
			pushl	%eax
			pushl	%ecx
			pushl	%edx
				pushl 	%esi						
				pushl	base2_offset
				call	update_cities_draw				
				addl	$8, %esp					
			popl	%edx
			popl	%ecx
			popl	%eax
		
	# check if base 3 is alive
	check_base_3:
		# initiailize %ebx to 1 (alive)
		movl	$1, %ebx
		movb	base_alive+2, %al
		cmpb	$0, %al
		## if != 0, base 3 is alive
		jne		update_cities_draw_base3_call
		## if = 0, base 3 is dead. %ebx = 0
		xorl	%ebx, %ebx
		# call "update_cities_draw_base3_call" function to draw base 3
		update_cities_draw_base3_call:
			pushl	%eax
			pushl	%ecx
			pushl	%edx
				pushl 	%ebx				
				pushl	base3_offset
				call	update_cities_draw				
				addl	$8, %esp				
			popl	%edx
			popl	%ecx
			popl	%eax
	
# "update_cities" is done. Restore everything
done_update_cities:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret
		
# void update_cities_draw(int base_offset, int dead_or_alive)
# update the status of the cities and draw it
# Registers: Standard C calling convention
# %edi used as base_offset  		  					-----#
# %esi used as dead_or_alive (1: alive; 0: dead) 		-----#
update_cities_draw:
# when "update_cities_draw" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi

	# retrieve the parameters
	movl	8(%ebp), %edi		#base_offset
	movl	12(%ebp), %esi		#dead_or_alive
	
	# call draw_city
	draw_city:
	# check if the city is alive
	cmpl	$1, %esi
	## if = base_pic, alive
	je		update_cities_draw_alive
	## if != base_pic, dead
		movb	dead_base_pic, %cl
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	dead_base_pic+1, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	dead_base_pic+2, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	dead_base_pic+3, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	dead_base_pic+4, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		jmp		done_update_cities
	
	update_cities_draw_alive:
		movb	base_pic, %cl
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	base_pic+1, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	base_pic+2, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	base_pic+3, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
		movb	base_pic+4, %cl
		addl	$2,	%edi
		movl	%edi, %eax
		call 	mp1_poke
		
# "update_cities_draw" is done. Restore everything
done_update_cities_draw:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret
		
# void update_crosshairs(void)
# update the crosshairs and draw it
# Registers: Standard C calling convention
# %eax is offset for mp1_poke 		  -----#
# %cl is token for mp1_poke 		  -----#
update_crosshairs:
# when "update_crosshairs" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# calculate the offset
		movl	crosshairs_y, %ebx
		movl	$80, %eax
		mull	%ebx
		movl	%eax, %ecx
		movl	crosshairs_x, %ebx
		addl	%ebx, %ecx	
		movl	$2, %eax
		mull	%ecx
		# insert the character
		movb	$CROSSHAIRS_ASCII, %cl
		# call mp1_poke
		call 	mp1_poke	
# "update_crosshairs" is done. Restore everything
done_update_crosshairs:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl_startgame(unsigned long ignore)
# initiailize the datas and move crosshairs into (40,12)
# 	inputs: ignore - just ignore it
# Registers: Standard C calling convention		
mp1_ioctl_startgame:
# when "mp1_ioctl_startgame" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# initiailize all data
	xorl 	%edi, %edi					
	movl	%edi, mp1_missile_list		# Head of list pointer
	movl	$0xFFFFFFFF, base_alive		# Array of 3 bytes, plus a padding byte
	movl	%edi, mp1_score				# Player's current score
	# move crosshairs into (40,12)
	movl	$40, crosshairs_x		
	movl	$12, crosshairs_y	
	
# "mp1_ioctl_startgame" is done. Restore everything
done_mp1_ioctl_startgame:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl_addmissile(struct missile* user_missile)
# add new missile to the game
# 	inputs: user_missile - pointer to a struct missile in user space
# %ebx store the parameter "user_missile"
# %esi store the result (memory address) of calling "mp1_malloc"
# %edi store the result of calling "mp1_copy_from_user"
# Registers: Standard C calling convention	
mp1_ioctl_addmissile:
# when "mp1_ioctl_addmissile" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# retrieve the parameter and store it into %ebx
	xorl	%ebx, %ebx
	movl 	8(%ebp), %ebx
	
	# allocate the memory first, calling "mp1_malloc"
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	$33				# parameter for mp1_malloc (for size of 33 bytes)
			call	mp1_malloc
			movl	%eax, %esi		# store the result of calling "mp1_malloc"
			addl	$4, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
	# check if the memory is successfully allocated (failed = 0 (null))
		cmpl	$0, %esi
		## if the memory allocate failed
		je		allocate_failed
		## if the memory is successfully allocated
	# copy the user's missile into a dynamically allocated buffer in kernel space (call "mp1_copy_from_user")
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	$33				# parameter for mp1_copy_from_user (for size of 33 bytes)
			pushl	%ebx			# parameter for mp1_copy_from_user (for address "from")
			pushl	%esi			# parameter for mp1_copy_from_user (for address "to")
			call	mp1_copy_from_user
			movl	%eax, %edi		# store the result of calling "mp1_malloc"
			addl	$12, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
	# check if the memory is successfully copied from user space (successfully when = 0)
		cmpl	$0, %edi
		## if the memory allocate failed
		jne		copy_failed
		## if the memory allocate successfully, add this missile into list and return 0
			movl	mp1_missile_list, %eax
			movl	%eax, NEXT(%esi)
		movl	%esi, mp1_missile_list
		xorl	%eax, %eax
		incl	test_list_long					#test
		jmp		done_mp1_ioctl_addmissile
	
	# copy_failed
	## if copy fail, free the memory allocated by calling "mp1_free"
	copy_failed:
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	%esi			# parameter for mp1_free (memory address)
			call	mp1_free
			addl	$4, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
	
	# allocate_failed
	## if allocate failed, return -1
	allocate_failed:
		xorl	%eax, %eax
		subl	$1, %eax
	
# "mp1_ioctl_addmissile" is done. Restore everything
done_mp1_ioctl_addmissile:
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl_movexhairs(unsigned long xhair_delta_packed)
# moves the crosshairs (redraw)
# 	inputs: xhair_delta_packed - low 16 bits for x, high 16 bits for y
# %ebx store the parameter "xhair_delta_packed"
# %cx store the 16 bits x-location-delta
# %dx store the 16 bits y-location-delta
# Registers: Standard C calling convention	
mp1_ioctl_movexhairs:
# when "mp1_ioctl_movexhairs" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# retrieve the parameter
	movl	8(%ebp), %ebx
	# store x-location in %cx
	movw	%bx, %cx
	# store y-location in %dx	
	movl	%ebx, %eax
	shrl	$16, %eax
	movw	%ax, %dx		

	# add the deltas into crosshairs_x and crosshairs_y
	addw	%cx, crosshairs_x
	addw	%dx, crosshairs_y
	
	# check if the new crosshairs' x-location exceeds the range
	check_movehairs_x_outside_range:
		# check if x-location < 0
		cmpw	$0, crosshairs_x
		## if x-location < 0
		jl		check_movehairs_x_less_0
		## if x-location >= 0, check if x-location > 79
		cmpw	$79, crosshairs_x
		## if x-location > 79
		jg		check_movehairs_x_greater_79
		## if x-location <= 79
		jmp		check_movehairs_y_outside_range
		## if x-location < 0, let x remain 0	
		check_movehairs_x_less_0:
			xorl	%eax, %eax
			movl	%eax, crosshairs_x
			jmp		check_movehairs_y_outside_range
		## if x-location > 79, let x remain 79	
		check_movehairs_x_greater_79:
			movw	$79, crosshairs_x
			jmp		check_movehairs_y_outside_range
			
	# check if the new crosshairs' y-location exceeds the range
	check_movehairs_y_outside_range:
		# check if y-location < 0
		cmpw	$0, crosshairs_y
		## if y-location < 0
		jl		check_movehairs_y_less_0
		## if y-location >= 0, check if y-location > 24
		cmpw	$24, crosshairs_y
		## if y-location > 24
		jg		check_movehairs_y_greater_24
		## if y-location <= 24
		jmp		done_mp1_ioctl_movexhairs
		## if y-location < 0, let y remain 0	
		check_movehairs_y_less_0:
			xorl	%eax, %eax
			movl	%eax, crosshairs_y
			jmp		done_mp1_ioctl_movexhairs
		## if y-location > 24, let y remain 24	
		check_movehairs_y_greater_24:
			movw	$24, crosshairs_y
			jmp		done_mp1_ioctl_movexhairs
	
	# assign crosshairs_x and crosshairs_y to %ecx and %edx
		movw	crosshairs_x, %cx
		movw	crosshairs_y, %dx
		
# "mp1_ioctl_movexhairs" is done. Restore everything
done_mp1_ioctl_movexhairs:
	xorl	%eax, %eax		# should never fail and return 0
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl_getstatus(unsigned long *user_status)
# allow user to retrieve the current score and the status of the three cities
# 	inputs: xhair_delta_packed - a pointer to a 32-bit integer in user space
# %ebx store the parameter "user_status"
# %ecx store the data which will be copied to the user. (lower 16-bit: the score; bits 16 17 18: status of the cities)
# %esi store the result of "mp1_copy_to_user"
# Registers: Standard C calling convention
mp1_ioctl_getstatus:
# when "mp1_ioctl_getstatus" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	# retrieve the parameter
	movl	8(%ebp), %ebx
	
	# initiailize %ecx
	xorl	%ecx, %ecx
	# copy mp1_score to %ecx
	movw	mp1_score, %cx
	
	# check if base 1 alive
		cmpb	$0, base_alive
	## if = 0, dead
		je		mp1_ioctl_getstatus_base1_dead
	## if != 0, alive and make bits 16 = 1
		orl		$0x00010000, %ecx
		jmp 	mp1_ioctl_getstatus_check_base2
	# dead and make bits 16 = 0
	mp1_ioctl_getstatus_base1_dead:
		andl	$0xFFFEFFFF, %ecx
		
	# check if base 2 alive
	mp1_ioctl_getstatus_check_base2:
		cmpb	$0, base_alive+1
	## if = 0, dead
		je		mp1_ioctl_getstatus_base2_dead
	## if != 0, alive and make bits 17 = 1
		orl		$0x00020000, %ecx
		jmp 	mp1_ioctl_getstatus_check_base3
	# dead and make bits 17 = 0
	mp1_ioctl_getstatus_base2_dead:
		andl	$0xFFFDFFFF, %ecx
		
	# check if base 3 alive
	mp1_ioctl_getstatus_check_base3:
		cmpb	$0, base_alive+2
	## if = 0, dead
		je		mp1_ioctl_getstatus_base3_dead
	## if != 0, alive and make bits 18 = 1
		orl		$0x00040000, %ecx
		jmp 	mp1_ioctl_getstatus_prepared_to_copy
	# dead and make bits 18 = 0
	mp1_ioctl_getstatus_base3_dead:
		andl	$0xFFFBFFFF, %ecx
	
	# after updating, here to copy to user space
	mp1_ioctl_getstatus_prepared_to_copy:
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	$4						# parameter 3, 4 bytes
			movl	%ecx, address_data_copy_to
			pushl	$address_data_copy_to	# parameter 2, *from	
			pushl	%ebx					# parameter 1, *to
			call	mp1_copy_to_user
			movl	%eax, %esi
			addl	$12, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
		
	# check if the memory is successfully copied to user space (successfully when = 0)
		xorl	%eax, %eax				# initiailize %eax (return value)
		cmpl	$0, %esi
		## if %esi = 0, success and return 0
		je		done_mp1_ioctl_getstatus
		## if %esi != 0, fail and return -1
		subl	$1, %eax
		
# "mp1_ioctl_getstatus" is done. Restore everything
done_mp1_ioctl_getstatus:	
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

# int mp1_ioctl_endgame(unsigned long ignore)
# free all memory
# 	inputs: ignore - just ignore it
# %ebx store the parameter "user_status"
# %ecx store the data which will be copied to the user. (lower 16-bit: the score; bits 16 17 18: status of the cities)
# %esi store the result of "mp1_copy_to_user"
# Registers: Standard C calling convention
mp1_ioctl_endgame:
# when "mp1_ioctl_endgame" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	movl	mp1_missile_list, %ebx
	# check if the current list exists 
	mp1_ioctl_endgame_check_list:
		cmpl	$0x0, %ebx
		## if = 0, no any list and jump to "done_mp1_ioctl_endgame"
		je		done_mp1_ioctl_endgame
		## if != 0, alter the connection and call "mp1_free" to free the memory
		movl	%ebx, %edi
		movl	NEXT(%ebx), %ebx
		movl	%ebx, mp1_missile_list
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			pushl	%edi
			call	mp1_free
			addl 	$4, %esp
		popl	%edx
		popl	%ecx
		popl	%eax
		# after freeing the memory, check if next list exists
		movl	mp1_missile_list, %ebx
		jmp		mp1_ioctl_endgame_check_list

# "mp1_ioctl_getstatus" is done. Restore everything
done_mp1_ioctl_endgame:	
	xorl	%eax, %eax		# assume return 0 denotes success
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret
		
# void clean_all_screen(void)
# clean all screen
# Registers: Standard C calling convention
clean_all_screen:
# when "mp1_ioctl_endgame" is called (callee-saved)
	pushl	%ebp	
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%esi
	pushl 	%edi
	
	movl	$2000, %ebx		#80*25
	xorl	%edi, %edi		#iteration = 0
	
	clean_all_screen_loop:
		cmpl	$0, %ebx
		## if %ebx = 0, end loop
		je		done_clean_all_screen
		## if %ebx != 0, execute loop
		pushl	%eax
		pushl	%ecx
		pushl	%edx
			movl	%edi, %eax
			movb	$SPACE, %cl
		
			call	mp1_poke
		popl	%edx
		popl	%ecx
		popl	%eax
		
		subl	$1, %ebx	#loop -= 1
		addl	$2, %edi	#iteration++
		
		jmp 	clean_all_screen_loop
	
# "clean_all_screen" is done. Restore everything
done_clean_all_screen:	
	leal	-12(%ebp), %esp
	popl	%edi
	popl	%esi
	popl	%ebx
		leave
		ret

