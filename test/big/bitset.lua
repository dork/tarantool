local SPACE_NO = 23
local INDEX_NO = 1

function fill(...)
	local nums = table.generate(arithmetic(...));
	table.shuffle(nums);
	for _k, v in ipairs(nums) do
		box.insert(SPACE_NO, v, v);
	end
	
-- 	box.insert(SPACE_NO, 0, 0)
-- 	box.insert(SPACE_NO, bit.lshift(1, 29), bit.lshift(1, 29))
-- 	box.insert(SPACE_NO, bit.lshift(1, 30), bit.lshift(1, 30))
end

function remove(...)
	local nums = table.generate(arithmetic(...));
	table.shuffle(nums);
	for _k, v in ipairs(nums) do
		box.delete(SPACE_NO, v);
	end
end

function clear()
	box.space[SPACE_NO]:truncate()
end

function test(...)
	iterate(SPACE_NO, INDEX_NO, 1, 2, ...);
end
