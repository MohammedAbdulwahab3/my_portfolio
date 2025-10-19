-- Realistic Seed Data for Portfolio
-- Run this AFTER running the fresh-setup.sql script

-- Insert realistic projects
INSERT INTO public.projects (title, description, tech_stack, image_url, demo_url, code_url, featured, slug, case_study_content, process_description, challenges, solutions, results, gallery_images) VALUES

('E-Commerce Dashboard', 'A comprehensive admin dashboard for managing online stores with real-time analytics, inventory management, and customer insights. Built with modern React architecture and optimized for performance.', 
ARRAY['React', 'TypeScript', 'Node.js', 'PostgreSQL', 'Redis', 'Docker'], 
'https://images.unsplash.com/photo-1551288049-bebda4e38f71?w=800&h=600&fit=crop', 
'https://demo-ecommerce-dashboard.vercel.app', 
'https://github.com/username/ecommerce-dashboard', 
true, 
'ecommerce-dashboard',
'This project was built to solve real inventory management challenges for small to medium businesses. The dashboard provides comprehensive analytics and streamlined operations.',
'Started with user research, created wireframes, developed MVP, iterative testing, and deployment with monitoring.',
'Complex state management, real-time data synchronization, performance optimization with large datasets, and responsive design across devices.',
'Implemented Redux Toolkit for state management, WebSocket connections for real-time updates, virtual scrolling for performance, and CSS Grid with Flexbox for responsive layouts.',
'40% reduction in inventory management time, 25% increase in operational efficiency, 99.9% uptime, and positive feedback from 50+ beta users.',
ARRAY['https://images.unsplash.com/photo-1551288049-bebda4e38f71?w=400&h=300&fit=crop', 'https://images.unsplash.com/photo-1460925895917-afdab827c52f?w=400&h=300&fit=crop']),

('Task Management Mobile App', 'Cross-platform mobile application for team collaboration and project management. Features include real-time chat, file sharing, and advanced project tracking with intuitive UI/UX.',
ARRAY['Flutter', 'Dart', 'Firebase', 'Node.js', 'MongoDB', 'WebSocket'],
'https://images.unsplash.com/photo-1611224923853-80b023f02d71?w=800&h=600&fit=crop',
'https://play.google.com/store/apps/details?id=com.taskmanager',
'https://github.com/username/task-manager-app',
true,
'task-management-app',
'Developed to address the need for seamless team collaboration in remote work environments. The app combines project management with real-time communication.',
'Market research, user persona development, UI/UX design, Flutter development, Firebase integration, testing, and App Store deployment.',
'Cross-platform consistency, real-time synchronization, offline functionality, push notifications, and app store approval process.',
'Used Flutter for consistent cross-platform UI, Firebase for real-time database and auth, implemented local SQLite for offline mode, and FCM for notifications.',
'10K+ downloads in first month, 4.8/5 app store rating, featured in "Productivity Apps" category, and adopted by 100+ teams.',
ARRAY['https://images.unsplash.com/photo-1611224923853-80b023f02d71?w=400&h=300&fit=crop', 'https://images.unsplash.com/photo-1512941937669-90a1b58e7e9c?w=400&h=300&fit=crop']),

('AI-Powered Analytics Platform', 'Machine learning platform that provides predictive analytics for business intelligence. Features automated report generation, data visualization, and custom ML model deployment.',
ARRAY['Python', 'Django', 'TensorFlow', 'React', 'PostgreSQL', 'Docker', 'AWS'],
'https://images.unsplash.com/photo-1551288049-bebda4e38f71?w=800&h=600&fit=crop',
'https://analytics-platform-demo.herokuapp.com',
'https://github.com/username/ai-analytics-platform',
true,
'ai-analytics-platform',
'Built to democratize AI-powered insights for businesses without dedicated data science teams. The platform makes complex analytics accessible through intuitive interfaces.',
'Requirements gathering, data architecture design, ML model development, web application development, cloud deployment, and user training.',
'Handling large datasets, model accuracy optimization, real-time processing, scalable architecture, and user-friendly ML interfaces.',
'Implemented data pipeline with Apache Airflow, used TensorFlow for ML models, Redis for caching, and AWS services for scalable deployment.',
'Processed 1M+ data points daily, achieved 94% model accuracy, reduced analysis time by 60%, and served 200+ enterprise clients.',
ARRAY['https://images.unsplash.com/photo-1551288049-bebda4e38f71?w=400&h=300&fit=crop', 'https://images.unsplash.com/photo-1504868584819-f8e8b4b6d7e3?w=400&h=300&fit=crop']),

('Real Estate Platform', 'Comprehensive real estate marketplace with advanced search filters, virtual tours, mortgage calculator, and agent management system. Optimized for both buyers and sellers.',
ARRAY['Next.js', 'TypeScript', 'Prisma', 'PostgreSQL', 'Stripe', 'MapBox'],
'https://images.unsplash.com/photo-1560518883-ce09059eeffa?w=800&h=600&fit=crop',
'https://realestate-platform.vercel.app',
'https://github.com/username/realestate-platform',
false,
'real-estate-platform',
'Created to modernize the real estate search experience with advanced filtering, virtual tours, and seamless transaction management.',
'Market analysis, competitor research, user journey mapping, full-stack development, payment integration, and SEO optimization.',
'Complex search algorithms, map integration, payment processing, image optimization, and mobile responsiveness.',
'Used Elasticsearch for advanced search, MapBox for interactive maps, Stripe for payments, and Next.js for SEO optimization.',
'50K+ property listings, 15K+ registered users, 30% increase in lead conversion, and 4.7/5 user satisfaction rating.',
ARRAY['https://images.unsplash.com/photo-1560518883-ce09059eeffa?w=400&h=300&fit=crop', 'https://images.unsplash.com/photo-1582407947304-fd86f028f716?w=400&h=300&fit=crop']),

('Fitness Tracking PWA', 'Progressive Web App for fitness enthusiasts with workout planning, nutrition tracking, progress analytics, and social features. Works offline and provides native app experience.',
ARRAY['Vue.js', 'PWA', 'IndexedDB', 'Chart.js', 'Service Workers', 'Firebase'],
'https://images.unsplash.com/photo-1571019613454-1cb2f99b2d8b?w=800&h=600&fit=crop',
'https://fitness-tracker-pwa.netlify.app',
'https://github.com/username/fitness-tracker-pwa',
false,
'fitness-tracker-pwa',
'Developed to provide a comprehensive fitness solution that works seamlessly across all devices with offline capabilities.',
'User research, PWA architecture planning, Vue.js development, offline functionality implementation, and performance optimization.',
'Offline data synchronization, performance on low-end devices, battery optimization, and engaging user experience.',
'Implemented Service Workers for offline functionality, IndexedDB for local storage, and optimized bundle size for performance.',
'25K+ active users, 90% user retention rate, featured in PWA showcase, and 4.6/5 rating on web app stores.',
ARRAY['https://images.unsplash.com/photo-1571019613454-1cb2f99b2d8b?w=400&h=300&fit=crop', 'https://images.unsplash.com/photo-1534438327276-14e5300c3a48?w=400&h=300&fit=crop']);

-- Insert realistic testimonials
INSERT INTO public.testimonials (name, position, company, content, avatar_url, rating, featured) VALUES

('Sarah Johnson', 'CTO', 'TechStart Inc.', 'Working with this developer was an absolute game-changer for our startup. They delivered a robust e-commerce platform that exceeded our expectations. The attention to detail, code quality, and communication throughout the project was exceptional. Our conversion rates increased by 35% after launch!', 'https://images.unsplash.com/photo-1494790108755-2616b612b786?w=150&h=150&fit=crop&crop=face', 5, true),

('Michael Chen', 'Product Manager', 'InnovateLab', 'The AI analytics platform they built for us has revolutionized how we make data-driven decisions. The intuitive interface makes complex machine learning accessible to our entire team. The project was delivered on time and within budget. Highly recommend for any AI/ML projects!', 'https://images.unsplash.com/photo-1472099645785-5658abf4ff4e?w=150&h=150&fit=crop&crop=face', 5, true),

('Emily Rodriguez', 'Founder', 'FitLife Solutions', 'The fitness tracking PWA they developed has been a huge success with our users. The offline functionality and smooth performance across devices is impressive. User engagement increased by 60% since launch. Professional, responsive, and delivers quality work consistently.', 'https://images.unsplash.com/photo-1438761681033-6461ffad8d80?w=150&h=150&fit=crop&crop=face', 5, true),

('David Thompson', 'VP Engineering', 'PropTech Ventures', 'Outstanding work on our real estate platform. The complex search functionality and map integration work flawlessly. The developer showed great problem-solving skills and delivered innovative solutions. The platform now handles 10K+ daily active users without any issues.', 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=150&h=150&fit=crop&crop=face', 5, false),

('Lisa Wang', 'CEO', 'TaskFlow Corp', 'The mobile app they built for our team collaboration needs has transformed our workflow. The real-time features work perfectly, and the UI/UX is intuitive. Our team productivity increased by 40%. Great communication and technical expertise throughout the project.', 'https://images.unsplash.com/photo-1487412720507-e7ab37603c6f?w=150&h=150&fit=crop&crop=face', 5, false),

('James Miller', 'Lead Developer', 'CodeCraft Agency', 'Collaborated on multiple projects and consistently impressed by the code quality and architectural decisions. Strong expertise in modern web technologies and best practices. A reliable partner for complex development challenges.', 'https://images.unsplash.com/photo-1500648767791-00dcc994a43e?w=150&h=150&fit=crop&crop=face', 5, false);

-- Insert realistic blog posts
INSERT INTO public.blog_posts (title, slug, excerpt, content, featured_image, published, tags, categories, meta_description, social_image) VALUES

('Building Scalable React Applications: Best Practices for 2024', 
'scalable-react-applications-2024', 
'Learn the essential patterns and practices for building React applications that can grow with your business needs. From component architecture to state management strategies.',
'<h2>Introduction</h2><p>Building scalable React applications requires careful planning and adherence to proven patterns. In this comprehensive guide, we''ll explore the key strategies that have proven successful in production environments.</p><h2>Component Architecture</h2><p>The foundation of any scalable React application lies in its component architecture. Here are the key principles:</p><ul><li><strong>Single Responsibility Principle:</strong> Each component should have one clear purpose</li><li><strong>Composition over Inheritance:</strong> Use composition to build complex UIs</li><li><strong>Prop Drilling Solutions:</strong> Implement Context API or state management libraries</li></ul><h2>State Management Strategies</h2><p>Choosing the right state management approach is crucial for scalability:</p><h3>Local State</h3><p>Use React''s built-in useState and useReducer for component-specific state that doesn''t need to be shared.</p><h3>Global State</h3><p>For application-wide state, consider:</p><ul><li>Redux Toolkit for complex applications</li><li>Zustand for simpler state needs</li><li>Context API for theme and user preferences</li></ul><h2>Performance Optimization</h2><p>Key techniques for maintaining performance as your app grows:</p><ul><li>Code splitting with React.lazy()</li><li>Memoization with React.memo() and useMemo()</li><li>Virtual scrolling for large lists</li><li>Image optimization and lazy loading</li></ul><h2>Testing Strategies</h2><p>A robust testing strategy is essential for scalable applications:</p><ul><li>Unit tests with Jest and React Testing Library</li><li>Integration tests for user workflows</li><li>E2E tests with Playwright or Cypress</li></ul><h2>Conclusion</h2><p>Building scalable React applications is an iterative process that requires continuous refinement. By following these best practices and staying updated with the React ecosystem, you can create applications that grow gracefully with your business needs.</p>',
'https://images.unsplash.com/photo-1633356122544-f134324a6cee?w=800&h=400&fit=crop',
true,
ARRAY['React', 'TypeScript', 'Performance'],
ARRAY['Web Development', 'Tutorials'],
'Learn essential patterns and best practices for building scalable React applications in 2024. Complete guide with code examples.',
'https://images.unsplash.com/photo-1633356122544-f134324a6cee?w=1200&h=630&fit=crop'),

('The Future of Mobile Development: Flutter vs React Native in 2024',
'flutter-vs-react-native-2024',
'A comprehensive comparison of Flutter and React Native for mobile development. Performance benchmarks, ecosystem analysis, and real-world project insights.',
'<h2>Introduction</h2><p>The mobile development landscape continues to evolve rapidly. Two frameworks have emerged as leaders in cross-platform development: Flutter and React Native. This analysis provides an updated comparison for 2024.</p><h2>Performance Comparison</h2><p>Both frameworks have made significant improvements in performance:</p><h3>Flutter Performance</h3><ul><li>Compiled to native ARM code</li><li>Consistent 60fps animations</li><li>Smaller app bundle sizes with tree shaking</li><li>Excellent performance on both iOS and Android</li></ul><h3>React Native Performance</h3><ul><li>New Architecture (Fabric + TurboModules) improves performance</li><li>Hermes JavaScript engine optimization</li><li>Better memory management</li><li>Improved startup times</li></ul><h2>Developer Experience</h2><h3>Flutter Advantages</h3><ul><li>Hot reload for instant feedback</li><li>Comprehensive widget library</li><li>Strong typing with Dart</li><li>Excellent debugging tools</li></ul><h3>React Native Advantages</h3><ul><li>JavaScript ecosystem familiarity</li><li>Large community and resources</li><li>Code sharing with web applications</li><li>Mature third-party library ecosystem</li></ul><h2>Ecosystem and Community</h2><p>Both frameworks have strong ecosystems, but with different strengths:</p><ul><li><strong>React Native:</strong> Larger community, more third-party packages, backed by Meta</li><li><strong>Flutter:</strong> Growing rapidly, strong Google support, comprehensive official packages</li></ul><h2>When to Choose Each Framework</h2><h3>Choose Flutter When:</h3><ul><li>You need pixel-perfect UI consistency</li><li>Performance is critical</li><li>You''re starting a new project</li><li>You want a single codebase for multiple platforms</li></ul><h3>Choose React Native When:</h3><ul><li>You have existing React/JavaScript expertise</li><li>You need extensive third-party integrations</li><li>You want to share code with web applications</li><li>You need platform-specific native modules</li></ul><h2>Conclusion</h2><p>Both Flutter and React Native are excellent choices for mobile development in 2024. The decision should be based on your team''s expertise, project requirements, and long-term goals. Consider prototyping with both frameworks to make an informed decision.</p>',
'https://images.unsplash.com/photo-1512941937669-90a1b58e7e9c?w=800&h=400&fit=crop',
true,
ARRAY['Flutter', 'React Native', 'Mobile Development'],
ARRAY['Mobile Development', 'Tutorials'],
'Comprehensive comparison of Flutter vs React Native for mobile development in 2024. Performance, ecosystem, and practical insights.',
'https://images.unsplash.com/photo-1512941937669-90a1b58e7e9c?w=1200&h=630&fit=crop'),

('Mastering TypeScript: Advanced Patterns for Better Code',
'mastering-typescript-advanced-patterns',
'Explore advanced TypeScript patterns that will make your code more robust, maintainable, and type-safe. From utility types to conditional types and beyond.',
'<h2>Introduction</h2><p>TypeScript has become an essential tool for JavaScript developers. Beyond basic type annotations, TypeScript offers powerful advanced features that can significantly improve code quality and developer experience.</p><h2>Utility Types</h2><p>TypeScript provides several built-in utility types that can transform existing types:</p><h3>Partial and Required</h3><pre><code>interface User {
  id: string;
  name: string;
  email: string;
}

// Make all properties optional
type PartialUser = Partial&lt;User&gt;;

// Make all properties required
type RequiredUser = Required&lt;PartialUser&gt;;</code></pre><h3>Pick and Omit</h3><pre><code>// Select specific properties
type UserSummary = Pick&lt;User, "id" | "name"&gt;;

// Exclude specific properties
type UserWithoutId = Omit&lt;User, "id"&gt;;</code></pre><h2>Conditional Types</h2><p>Conditional types allow you to create types that depend on conditions:</p><pre><code>type ApiResponse&lt;T&gt; = T extends string
  ? { message: T }
  : { data: T };

type StringResponse = ApiResponse&lt;string&gt;; // { message: string }
type UserResponse = ApiResponse&lt;User&gt;; // { data: User }</code></pre><h2>Mapped Types</h2><p>Create new types by transforming properties of existing types:</p><pre><code>type Readonly&lt;T&gt; = {
  readonly [P in keyof T]: T[P];
};

type Optional&lt;T&gt; = {
  [P in keyof T]?: T[P];
};</code></pre><h2>Template Literal Types</h2><p>Combine string literals in type definitions:</p><pre><code>type EventName&lt;T extends string&gt; = `on${Capitalize&lt;T&gt;}`;
type ButtonEvents = EventName&lt;"click" | "hover"&gt;; // "onClick" | "onHover"</code></pre><h2>Advanced Function Types</h2><p>Create flexible and type-safe function signatures:</p><pre><code>type AsyncFunction&lt;T extends any[], R&gt; = (...args: T) =&gt; Promise&lt;R&gt;;

type EventHandler&lt;T = Event&gt; = (event: T) =&gt; void;

type Middleware&lt;T, R&gt; = (input: T, next: () =&gt; R) =&gt; R;</code></pre><h2>Best Practices</h2><ul><li>Use strict TypeScript configuration</li><li>Prefer type assertions over any</li><li>Leverage type guards for runtime safety</li><li>Use branded types for domain modeling</li><li>Implement proper error handling with Result types</li></ul><h2>Conclusion</h2><p>Mastering these advanced TypeScript patterns will help you write more robust and maintainable code. Start incorporating these techniques gradually into your projects to see immediate benefits in code quality and developer experience.</p>',
'https://images.unsplash.com/photo-1516116216624-53e697fedbea?w=800&h=400&fit=crop',
true,
ARRAY['TypeScript', 'JavaScript', 'Programming'],
ARRAY['Web Development', 'Tutorials'],
'Master advanced TypeScript patterns including utility types, conditional types, and mapped types for better code quality.',
'https://images.unsplash.com/photo-1516116216624-53e697fedbea?w=1200&h=630&fit=crop'),

('Building a Career in Tech: From Bootcamp to Senior Developer',
'building-career-tech-bootcamp-senior',
'A practical roadmap for advancing your tech career. Learn the skills, strategies, and mindset needed to grow from junior to senior developer.',
'<h2>Introduction</h2><p>The journey from bootcamp graduate to senior developer can seem daunting, but with the right approach and mindset, it''s entirely achievable. This guide outlines a practical roadmap based on real industry experience.</p><h2>The Learning Path</h2><h3>Foundation Phase (0-6 months)</h3><ul><li>Master fundamental programming concepts</li><li>Build a strong portfolio with diverse projects</li><li>Learn version control (Git) thoroughly</li><li>Understand basic software development lifecycle</li><li>Practice coding challenges and algorithms</li></ul><h3>Growth Phase (6-18 months)</h3><ul><li>Deepen framework knowledge (React, Angular, Vue, etc.)</li><li>Learn backend development and databases</li><li>Understand testing methodologies</li><li>Contribute to open source projects</li><li>Start mentoring junior developers</li></ul><h3>Specialization Phase (18+ months)</h3><ul><li>Choose a specialization (Frontend, Backend, DevOps, etc.)</li><li>Learn system design and architecture</li><li>Develop leadership and communication skills</li><li>Stay updated with industry trends</li><li>Build a professional network</li></ul><h2>Essential Technical Skills</h2><h3>Core Programming</h3><ul><li>Data structures and algorithms</li><li>Object-oriented and functional programming</li><li>Design patterns and architectural principles</li><li>Performance optimization techniques</li></ul><h3>Tools and Technologies</h3><ul><li>Version control systems (Git, GitHub)</li><li>CI/CD pipelines and deployment</li><li>Cloud platforms (AWS, Azure, GCP)</li><li>Containerization (Docker, Kubernetes)</li><li>Monitoring and logging tools</li></ul><h2>Soft Skills Development</h2><ul><li><strong>Communication:</strong> Learn to explain technical concepts clearly</li><li><strong>Problem-solving:</strong> Develop systematic debugging approaches</li><li><strong>Collaboration:</strong> Work effectively in team environments</li><li><strong>Time management:</strong> Balance multiple projects and deadlines</li><li><strong>Continuous learning:</strong> Stay curious and adaptable</li></ul><h2>Building Your Professional Network</h2><ul><li>Attend local meetups and conferences</li><li>Participate in online communities (Stack Overflow, Reddit, Discord)</li><li>Connect with colleagues and industry professionals on LinkedIn</li><li>Share your knowledge through blog posts and talks</li><li>Find mentors and become a mentor to others</li></ul><h2>Career Advancement Strategies</h2><h3>Performance Excellence</h3><ul><li>Consistently deliver high-quality work</li><li>Take ownership of projects and outcomes</li><li>Proactively identify and solve problems</li><li>Document your achievements and impact</li></ul><h3>Visibility and Recognition</h3><ul><li>Volunteer for challenging projects</li><li>Share knowledge with your team</li><li>Participate in code reviews and technical discussions</li><li>Present at team meetings and company events</li></ul><h2>Common Pitfalls to Avoid</h2><ul><li>Focusing only on technical skills while neglecting soft skills</li><li>Not seeking feedback regularly</li><li>Comparing yourself to others instead of focusing on growth</li><li>Avoiding challenging projects due to fear of failure</li><li>Not investing in continuous learning</li></ul><h2>Conclusion</h2><p>Building a successful career in tech requires a combination of technical expertise, soft skills, and strategic thinking. Focus on continuous learning, building relationships, and delivering value. Remember that career growth is a marathon, not a sprint. Stay persistent, be patient with yourself, and celebrate small wins along the way.</p>',
'https://images.unsplash.com/photo-1522202176988-66273c2fd55f?w=800&h=400&fit=crop',
true,
ARRAY['Career', 'Programming', 'Professional Development'],
ARRAY['Career', 'Tutorials'],
'Complete roadmap for advancing your tech career from bootcamp to senior developer. Practical advice and actionable strategies.',
'https://images.unsplash.com/photo-1522202176988-66273c2fd55f?w=1200&h=630&fit=crop');

-- Insert resume data
INSERT INTO public.resume (full_name, email, phone, summary, skills, experience, education, certifications) VALUES
('Alex Johnson', 'alex.johnson@email.com', '+1 (555) 123-4567', 
'Passionate Full-Stack Developer with 5+ years of experience building scalable web applications and mobile solutions. Expertise in React, Node.js, and cloud technologies. Proven track record of delivering high-quality products that drive business growth and improve user experience.',
'{
  "technical": [
    "JavaScript/TypeScript", "React/Next.js", "Node.js/Express", "Python/Django", 
    "PostgreSQL/MongoDB", "AWS/Azure", "Docker/Kubernetes", "Git/GitHub"
  ],
  "frameworks": [
    "React", "Vue.js", "Angular", "Next.js", "Express.js", "Django", "Flutter", "React Native"
  ],
  "tools": [
    "VS Code", "Docker", "Kubernetes", "Jenkins", "Webpack", "Vite", "Figma", "Postman"
  ],
  "soft_skills": [
    "Team Leadership", "Problem Solving", "Communication", "Project Management", "Mentoring"
  ]
}',
'{
  "positions": [
    {
      "title": "Senior Full-Stack Developer",
      "company": "TechInnovate Solutions",
      "location": "San Francisco, CA",
      "duration": "2022 - Present",
      "responsibilities": [
        "Lead development of microservices architecture serving 100K+ daily users",
        "Mentor junior developers and conduct code reviews",
        "Architect and implement CI/CD pipelines reducing deployment time by 60%",
        "Collaborate with product team to define technical requirements and roadmap"
      ],
      "achievements": [
        "Reduced application load time by 40% through performance optimization",
        "Led migration to cloud infrastructure saving $50K annually",
        "Implemented automated testing increasing code coverage to 95%"
      ]
    },
    {
      "title": "Full-Stack Developer",
      "company": "StartupXYZ",
      "location": "Austin, TX",
      "duration": "2020 - 2022",
      "responsibilities": [
        "Developed and maintained React-based web applications",
        "Built RESTful APIs using Node.js and Express",
        "Implemented real-time features using WebSocket technology",
        "Collaborated with designers to create responsive user interfaces"
      ],
      "achievements": [
        "Delivered 15+ feature releases on schedule",
        "Improved user engagement by 35% through UX enhancements",
        "Reduced bug reports by 50% through comprehensive testing"
      ]
    },
    {
      "title": "Junior Developer",
      "company": "WebCraft Agency",
      "location": "Remote",
      "duration": "2019 - 2020",
      "responsibilities": [
        "Developed responsive websites using HTML, CSS, and JavaScript",
        "Integrated third-party APIs and payment systems",
        "Participated in agile development processes",
        "Provided technical support and maintenance for client websites"
      ],
      "achievements": [
        "Successfully delivered 20+ client projects",
        "Achieved 98% client satisfaction rating",
        "Reduced development time by 25% through code reusability"
      ]
    }
  ]
}',
'{
  "degrees": [
    {
      "degree": "Bachelor of Science in Computer Science",
      "institution": "University of Technology",
      "location": "California, USA",
      "graduation_year": "2019",
      "gpa": "3.8/4.0",
      "relevant_coursework": [
        "Data Structures and Algorithms",
        "Software Engineering",
        "Database Systems",
        "Web Development",
        "Mobile App Development"
      ]
    }
  ],
  "bootcamps": [
    {
      "program": "Full-Stack Web Development Bootcamp",
      "institution": "CodeAcademy Pro",
      "completion_year": "2018",
      "duration": "6 months",
      "focus_areas": [
        "JavaScript/React",
        "Node.js/Express",
        "MongoDB",
        "RESTful APIs"
      ]
    }
  ]
}',
'{
  "certifications": [
    {
      "name": "AWS Certified Solutions Architect",
      "issuer": "Amazon Web Services",
      "date": "2023",
      "credential_id": "AWS-SAA-2023-001234",
      "expiry": "2026"
    },
    {
      "name": "Google Cloud Professional Developer",
      "issuer": "Google Cloud",
      "date": "2022",
      "credential_id": "GCP-PD-2022-005678",
      "expiry": "2025"
    },
    {
      "name": "Certified Kubernetes Administrator",
      "issuer": "Cloud Native Computing Foundation",
      "date": "2023",
      "credential_id": "CKA-2023-009876",
      "expiry": "2026"
    }
  ],
  "courses": [
    {
      "name": "Advanced React Patterns",
      "provider": "Frontend Masters",
      "completion_date": "2023"
    },
    {
      "name": "System Design Interview Course",
      "provider": "Educative.io",
      "completion_date": "2023"
    }
  ]
}');

-- Success message
SELECT 'Seed data inserted successfully! Your portfolio now has realistic content.' as message;
