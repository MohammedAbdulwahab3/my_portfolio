-- Add admin role to your user
-- Replace 'your-email@example.com' with your actual email address

-- First, find your user ID (run this query and note the ID)
SELECT id, email FROM auth.users WHERE email = 'your-email@example.com';

-- Then insert the admin role (replace the UUID with your actual user ID from above)
INSERT INTO public.user_roles (user_id, role) 
VALUES ('your-user-id-here', 'admin'::app_role)
ON CONFLICT (user_id, role) DO NOTHING;

-- Verify the role was added
SELECT ur.*, u.email 
FROM public.user_roles ur 
JOIN auth.users u ON ur.user_id = u.id 
WHERE u.email = 'your-email@example.com';
